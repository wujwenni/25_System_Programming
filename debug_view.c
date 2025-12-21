#include "debug_view.h"
#include "ui_helpers.h"
#include <stdio.h>
#include <string.h>

void dv_init(DebugView *dv) {
    memset(dv, 0, sizeof(DebugView));
    dbg_init(&dv->debugger);
    dv->source_loaded = 0;
    dv->scroll_offset = 0;
    memset(dv->compile_error, 0, sizeof(dv->compile_error));
}

void dv_set_compile_error(DebugView *dv, const char *error_msg) {
    if (error_msg) {
        strncpy(dv->compile_error, error_msg, sizeof(dv->compile_error) - 1);
        dv->compile_error[sizeof(dv->compile_error) - 1] = '\0';
    }
}

int dv_load_program(DebugView *dv, const char *executable_path, const char *source_path) {
    FILE *f = fopen(source_path, "r");
    if (!f) {
        return -1;
    }

    dv->source_line_count = 0;
    while (dv->source_line_count < 2000 &&
           fgets(dv->source_lines[dv->source_line_count], 256, f)) {
        dv->source_lines[dv->source_line_count][strcspn(dv->source_lines[dv->source_line_count], "\n")] = 0;
        dv->source_line_count++;
    }
    fclose(f);

    dv->source_loaded = 1;

    return dbg_load_program(&dv->debugger, executable_path, source_path);
}

void dv_draw(DebugView *dv, WINDOW *win_code, WINDOW *win_output, WINDOW *win_info) {
    int start_y, start_x, height, width;

    dbg_read_output(&dv->debugger);

    ui_get_usable_area(win_code, &start_y, &start_x, &height, &width);
    ui_draw_window(win_code, "SOURCE CODE");

    if (!dv->source_loaded) {
        wattron(win_code, COLOR_PAIR(COLOR_FILE) | A_DIM);
        ui_safe_print(win_code, start_y + height/2, start_x, "No source loaded");
        wattroff(win_code, COLOR_PAIR(COLOR_FILE) | A_DIM);
    } else {
        for (int i = 0; i < height && (dv->scroll_offset + i) < dv->source_line_count; i++) {
            int line_num = dv->scroll_offset + i + 1;
            int is_current = (line_num == dv->debugger.current_line);

            char line_buf[512];
            snprintf(line_buf, sizeof(line_buf), " %3d  %s",
                    line_num, dv->source_lines[dv->scroll_offset + i]);

            if (is_current) {
                wattron(win_code, COLOR_PAIR(COLOR_SELECTED) | A_BOLD | A_REVERSE);
                char arrow_line[512];
                snprintf(arrow_line, sizeof(arrow_line), ">>> %3d  %s",
                        line_num, dv->source_lines[dv->scroll_offset + i]);

                int max_x = getmaxx(win_code);
                mvwprintw(win_code, start_y + i, 1, "%-*s", max_x - 2, arrow_line);
                wattroff(win_code, COLOR_PAIR(COLOR_SELECTED) | A_BOLD | A_REVERSE);
            } else {
                wattron(win_code, COLOR_PAIR(COLOR_FILE));
                ui_safe_print(win_code, start_y + i, start_x, line_buf);
                wattroff(win_code, COLOR_PAIR(COLOR_FILE));
            }
        }
    }
    ui_get_usable_area(win_output, &start_y, &start_x, &height, &width);
    ui_draw_window(win_output, "PROGRAM OUTPUT");

    if (dv->compile_error[0] != '\0') {
        wattron(win_output, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        ui_safe_print(win_output, start_y, start_x, "COMPILATION ERROR:");
        wattroff(win_output, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);

        int y = start_y + 1;
        char *line_start = dv->compile_error;
        char *line_end;

        wattron(win_output, COLOR_PAIR(COLOR_FILE));
        while (y < start_y + height && *line_start != '\0') {
            line_end = strchr(line_start, '\n');
            if (line_end) {
                *line_end = '\0';
                ui_safe_print(win_output, y++, start_x, line_start);
                *line_end = '\n';
                line_start = line_end + 1;
            } else {
                ui_safe_print(win_output, y++, start_x, line_start);
                break;
            }
        }
        wattroff(win_output, COLOR_PAIR(COLOR_FILE));
    }
    else if (dv->debugger.output_length > 0) {
        int y = start_y;
        char *line_start = dv->debugger.output_buffer;
        char *line_end;

        while (y < start_y + height && line_start < dv->debugger.output_buffer + dv->debugger.output_length) {
            line_end = strchr(line_start, '\n');
            if (line_end) {
                *line_end = '\0';
                ui_safe_print(win_output, y++, start_x, line_start);
                *line_end = '\n';
                line_start = line_end + 1;
            } else {
                ui_safe_print(win_output, y++, start_x, line_start);
                break;
            }
        }
    } else {
        wattron(win_output, A_DIM);
        ui_safe_print(win_output, start_y, start_x, "(no output yet)");
        wattroff(win_output, A_DIM);
    }
    ui_get_usable_area(win_info, &start_y, &start_x, &height, &width);
    ui_draw_window(win_info, "DEBUG INFO");

    int y = start_y;

    wattron(win_info, COLOR_PAIR(COLOR_HEADER));
    char status[128];
    snprintf(status, sizeof(status), "State: %s", dbg_state_string(dv->debugger.state));
    ui_safe_print(win_info, y++, start_x, status);
    wattroff(win_info, COLOR_PAIR(COLOR_HEADER));

    if (dv->debugger.error_message[0] != '\0') {
        wattron(win_info, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        char error_line[256];
        snprintf(error_line, sizeof(error_line), "ERROR: %s", dv->debugger.error_message);
        ui_safe_print(win_info, y++, start_x, error_line);
        wattroff(win_info, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
    }
    y++;

    wattron(win_info, COLOR_PAIR(COLOR_FILE));
    char exec_info[64];
    snprintf(exec_info, sizeof(exec_info), "Line: %d / %d | Steps: %d",
             dv->debugger.current_line, dv->source_line_count,
             dv->debugger.instruction_count);
    ui_safe_print(win_info, y++, start_x, exec_info);
    wattroff(win_info, COLOR_PAIR(COLOR_FILE));
    y++;

    wattron(win_info, COLOR_PAIR(COLOR_HEADER));
    ui_safe_print(win_info, y++, start_x, "Controls:");
    wattroff(win_info, COLOR_PAIR(COLOR_HEADER));

    if (dv->compile_error[0] != '\0') {
        wattron(win_info, A_DIM);
        ui_safe_print(win_info, y++, start_x, " r - Run/Start (fix errors first)");
        ui_safe_print(win_info, y++, start_x, " n - Next");
        ui_safe_print(win_info, y++, start_x, " s - Step");
        wattroff(win_info, A_DIM);
    }
    else if (dv->debugger.state == DBG_STATE_NOT_STARTED ||
        dv->debugger.state == DBG_STATE_EXITED) {
        wattron(win_info, COLOR_PAIR(COLOR_FILE) | A_BOLD);
        ui_safe_print(win_info, y++, start_x, " r - Run/Start");
        wattroff(win_info, COLOR_PAIR(COLOR_FILE) | A_BOLD);
        wattron(win_info, A_DIM);
        ui_safe_print(win_info, y++, start_x, " n - Next");
        ui_safe_print(win_info, y++, start_x, " s - Step");
        wattroff(win_info, A_DIM);
    } else {
        ui_safe_print(win_info, y++, start_x, " r - Run/Start");
        wattron(win_info, COLOR_PAIR(COLOR_FILE) | A_BOLD);
        ui_safe_print(win_info, y++, start_x, " n - Next");
        ui_safe_print(win_info, y++, start_x, " s - Step");
        wattroff(win_info, COLOR_PAIR(COLOR_FILE) | A_BOLD);
    }

    ui_safe_print(win_info, y++, start_x, " Up/Dn - Navigate");
    ui_safe_print(win_info, y++, start_x, " ESC - Exit debug mode");

    wattroff(win_info, COLOR_PAIR(COLOR_FILE));
}

int dv_handle_key(DebugView *dv, int key) {
    switch (key) {
        case 27:
            dbg_stop(&dv->debugger);
            return 1;

        case 'r':
        case 'R':
            if (dv->compile_error[0] != '\0') {
                return 0;
            }
            if (dv->debugger.state == DBG_STATE_NOT_STARTED ||
                dv->debugger.state == DBG_STATE_EXITED) {
                dbg_start(&dv->debugger);
                if (dv->debugger.current_line > 0 && dv->debugger.current_line <= dv->source_line_count) {
                    dv->scroll_offset = dv->debugger.current_line - 1;
                    if (dv->scroll_offset < 0) dv->scroll_offset = 0;
                }
            }
            return 0;

        case 'n':
        case 'N':
            if (dv->debugger.state == DBG_STATE_STOPPED) {
                dbg_step_line(&dv->debugger);
                if (dv->debugger.current_line > dv->scroll_offset + 20) {
                    dv->scroll_offset = dv->debugger.current_line - 10;
                }
            }
            return 0;

        case 's':
        case 'S':
            if (dv->debugger.state == DBG_STATE_STOPPED) {
                dbg_step_line(&dv->debugger);
                if (dv->debugger.current_line > dv->scroll_offset + 20) {
                    dv->scroll_offset = dv->debugger.current_line - 10;
                }
            }
            return 0;

        case KEY_UP:
            if (dv->scroll_offset > 0) {
                dv->scroll_offset--;
            }
            return 0;

        case KEY_DOWN:
            if (dv->scroll_offset < dv->source_line_count - 20) {
                dv->scroll_offset++;
            }
            return 0;

        case KEY_NPAGE:
            dv->scroll_offset += 10;
            if (dv->scroll_offset > dv->source_line_count - 20) {
                dv->scroll_offset = dv->source_line_count - 20;
            }
            if (dv->scroll_offset < 0) dv->scroll_offset = 0;
            return 0;

        case KEY_PPAGE:
            dv->scroll_offset -= 10;
            if (dv->scroll_offset < 0) dv->scroll_offset = 0;
            return 0;
    }

    if (dv->debugger.state == DBG_STATE_EXITED) {
        return 2;
    }

    return 0;
}
