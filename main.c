#include <ncurses.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filemanager.h"
#include "code_view.h"
#include "ui_helpers.h"
#include "control_panel.h"
#include "debug_view.h"

typedef enum {
    MODE_BROWSE,
    MODE_DEBUG
} AppMode;

char* run_cmd(const char *cmd) {
    static char output[65536];
    output[0] = '\0';

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        strcpy(output, "exec failed");
        return output;
    }

    char line[1024];
    int total_len = 0;
    while (fgets(line, 1024, fp) != NULL) {
        int line_len = strlen(line);
        if (total_len + line_len < 65535) {
            strcat(output, line);
            total_len += line_len;
        } else {
            strcat(output, "\n...(truncated)");
            break;
        }
    }

    pclose(fp);
    return output;
}

void execute_vim(const char *cmd) {
    def_prog_mode();
    endwin();
    system(cmd);
    reset_prog_mode();
    refresh();
}

void draw_statusbar(int line, const char *msg) {
    attron(COLOR_PAIR(COLOR_STATUSBAR));
    mvprintw(line, 0, "%s", msg);
    clrtoeol();
    attroff(COLOR_PAIR(COLOR_STATUSBAR));
}

void calculate_layout(int *height, int *left_width, int *mid_width, int *right_width) {
    *height = LINES - 1;
    *left_width = COLS / 3;
    *mid_width = COLS / 3;
    *right_width = COLS - *left_width - *mid_width;
}

int main(void) {
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    mousemask(ALL_MOUSE_EVENTS, NULL);
    mouseinterval(0);
    ui_init_colors();
    clear();
    refresh();

    int height, left_width, mid_width, right_width;
    calculate_layout(&height, &left_width, &mid_width, &right_width);

    WINDOW* winleft = newwin(height, left_width, 0, 0);
    WINDOW* winmid = newwin(height, mid_width, 0, left_width);
    WINDOW* winright = newwin(height, right_width, 0, left_width + mid_width);

    const char *home_path = getenv("HOME");
    if (!home_path) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home_path = pw->pw_dir;
    }
    if (!home_path) {
        endwin();
        printf("Cannot find HOME\n");
        return 1;
    }

    FileManager fm;
    fm_init(&fm, home_path);
    fm_refresh_list(&fm);

    CodeView cv;
    cv_init(&cv);

    ControlPanel cp;
    cp_init(&cp);
    cp_set_current_dir(&cp, fm.cur_path);

    DebugView dv;
    dv_init(&dv);

    AppMode mode = MODE_BROWSE;

    MEVENT ev;
    int ch;
    while (1) {
        if (mode == MODE_DEBUG) {
            werase(winleft);
            werase(winright);
            werase(winmid); 
            dv_draw(&dv, winleft, winmid, winright);
            wrefresh(winleft);
            wrefresh(winmid);
            wrefresh(winright);

            char status[1024];
            snprintf(status, sizeof(status), " DEBUG MODE | State: %s | ESC:Exit | r:Run n:Next s:Step",
                     dbg_state_string(dv.debugger.state));
            draw_statusbar(LINES - 1, status);
            refresh();
        } else {
            fm_draw(&fm, winleft);
            wrefresh(winleft);

            cv_draw(&cv, winmid);
            wrefresh(winmid);

            cp_draw(&cp, winright);
            wrefresh(winright);

            char status[1024];
            const char *mode_str = "BROWSE";
            if (cp.mode == CP_MODE_CMD_INPUT) mode_str = "CMD";
            else if (cp.mode == CP_MODE_CMD_OUTPUT) mode_str = "OUTPUT";
            snprintf(status, sizeof(status), " [%s] | Files: %d | Mode: %s | PgUp/Dn:Scroll d:Debug v:Vim q:Quit",
                     fm.cur_path, fm.count, mode_str);
            draw_statusbar(LINES - 1, status);
            refresh();
        }

        ch = getch();

        if (mode == MODE_DEBUG) {
            int result = dv_handle_key(&dv, ch);
            if (result == 1) {
                mode = MODE_BROWSE;
                // Thoroughly clear the screen when exiting debug mode
                werase(winleft);
                werase(winmid);
                werase(winright);
                wrefresh(winleft);
                wrefresh(winmid);
                wrefresh(winright);
                clear();
                clearok(stdscr, TRUE);
                refresh();
            }
            continue;
        }
        if (ch == 'q' || ch == 'Q') {
            if (cp.mode == CP_MODE_NORMAL) {
                break;
            }
        }

        if (ch == KEY_RESIZE) {
            calculate_layout(&height, &left_width, &mid_width, &right_width);

            wresize(winleft, height, left_width);
            mvwin(winleft, 0, 0);

            wresize(winmid, height, mid_width);
            mvwin(winmid, 0, left_width);

            wresize(winright, height, right_width);
            mvwin(winright, 0, left_width + mid_width);

            rebuild_layout(&cv, mid_width);

            clear();
            refresh();
            continue;
        }
        char cmd_output[1024];
        int cp_result = cp_handle_key(&cp, ch, cmd_output, sizeof(cmd_output));
        if (cp_result == 1) {
            execute_vim(cmd_output);
            clear();
            refresh();
            continue;
        } else if (cp_result == 2) {
            char full_cmd[2048];
            if (cp.current_dir[0] != '\0') {
                snprintf(full_cmd, 2048, "cd \"%s\" && %s", cp.current_dir, cmd_output);
            } else {
                strncpy(full_cmd, cmd_output, 2047);
                full_cmd[2047] = '\0';
            }

            char *output = run_cmd(full_cmd);
            cp_set_output(&cp, cmd_output, output);
            clear();
            refresh();
            continue;
        }
        if (ch == 'd' || ch == 'D') {
            if (cp.mode == CP_MODE_NORMAL && cp.selected_file[0]) {
                const char *ext = strrchr(cp.selected_file, '.');
                if (ext && strcmp(ext, ".c") == 0) {
                    char exe_path[1024];
                    strncpy(exe_path, cp.selected_file, sizeof(exe_path) - 1);
                    char *dot = strrchr(exe_path, '.');
                    if (dot) *dot = '\0';

                    draw_statusbar(LINES - 1, " Compiling with debug info... Please wait.");
                    refresh();

                    char compile_cmd[2048];
                    snprintf(compile_cmd, sizeof(compile_cmd),
                            "gcc -g -O0 -no-pie -o %s %s 2>&1", exe_path, cp.selected_file);

                    char *compile_output = run_cmd(compile_cmd);

                    dv_init(&dv);

                    if (access(exe_path, X_OK) == 0) {
                        if (strlen(compile_output) > 0) {
                            cp_set_output(&cp, compile_cmd, compile_output);
                        }
                        if (dv_load_program(&dv, exe_path, cp.selected_file) == 0) {
                            mode = MODE_DEBUG;
                            clear();
                            refresh();
                            continue;
                        }
                    } else {
                        cp_set_output(&cp, compile_cmd, compile_output);

                        dv_load_program(&dv, "", cp.selected_file);
                        dv_set_compile_error(&dv, compile_output);

                        mode = MODE_DEBUG;
                        clear();
                        refresh();
                        continue;
                    }
                    clear();
                    refresh();
                }
            }
        }
        if (cp.mode == CP_MODE_NORMAL) {
            char sel_path[1024];
            int r = fm_handle_key(&fm, ch, sel_path, sizeof(sel_path));
            if (r == 1) {
                strncpy(fm.cur_path, sel_path, sizeof(fm.cur_path)-1);
                fm.cur_path[sizeof(fm.cur_path)-1] = '\0';
                fm_refresh_list(&fm);
                cv_init(&cv);
                cp_set_selected_file(&cp, NULL);
                cp_set_current_dir(&cp, fm.cur_path);
            } else if (r == 2) {
                cv_load(&cv, sel_path);
                rebuild_layout(&cv, mid_width);
                cp_set_selected_file(&cp, sel_path);
            }

            if (ch == KEY_PPAGE || ch == KEY_NPAGE || ch == KEY_HOME || ch == KEY_END) {
                cv_handle_key(&cv, ch);
            }
        }
        if (cp.mode == CP_MODE_NORMAL && ch == KEY_MOUSE) {
            if (getmouse(&ev) == OK) {
                char sel_path[1024];
                int r = fm_handle_mouse(&fm, &ev, left_width, sel_path, sizeof(sel_path));
                if (r == 1) {
                    strncpy(fm.cur_path, sel_path, sizeof(fm.cur_path)-1);
                    fm.cur_path[sizeof(fm.cur_path)-1] = '\0';
                    fm_refresh_list(&fm);
                    cv_init(&cv);
                    cp_set_selected_file(&cp, NULL);
                    cp_set_current_dir(&cp, fm.cur_path);
                } else if (r == 2) {
                    cv_load(&cv, sel_path);
                    rebuild_layout(&cv, mid_width);
                    cp_set_selected_file(&cp, sel_path);
                }
            }
        }
    }
    fm_cleanup(&fm);
    delwin(winleft);
    delwin(winmid);
    delwin(winright);
    endwin();
    return 0;
}