#include <ncurses.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_ITEMS 1024
#define LIST_START_ROW 2
#define NAME_START_COL 2

int not_hidden(const struct dirent *e) { return e->d_name[0] != '.'; }

int get_dir_list(const char *path, char **names, int *isdir, int *name_len) {
    struct dirent **namelist;
    int n = scandir(path, &namelist, not_hidden, alphasort);
    int count = 0;
    for (int i = 0; i < n && count < MAX_ITEMS-1; i++) {
        names[count] = strdup(namelist[i]->d_name);
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", path, namelist[i]->d_name);
        struct stat st;
        if (namelist[i]->d_type == DT_UNKNOWN)
            isdir[count] = (!stat(full, &st) && S_ISDIR(st.st_mode));
        else
            isdir[count] = (namelist[i]->d_type == DT_DIR);
        name_len[count] = strlen(namelist[i]->d_name) + (isdir[count] ? 1 : 0);
        free(namelist[i]);
        count++;
    }
    free(namelist);
    return count;
}

int main() {
    initscr(); noecho(); cbreak(); keypad(stdscr, TRUE); curs_set(0);
    mousemask(ALL_MOUSE_EVENTS, NULL); mouseinterval(0);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);   // 강조 색상
    init_pair(2, COLOR_WHITE, COLOR_BLACK);  // 기본 색상
    init_pair(3, COLOR_CYAN, COLOR_BLACK);   // 미드/라이트 임시 텍스트

    int height = LINES - 2;
    int mid_width = COLS / 3;
    int right_width = COLS / 3;
    int left_width = COLS - mid_width - right_width;
    WINDOW *win_left = newwin(height, left_width, 0, 0);
    WINDOW *win_mid = newwin(height, mid_width, 0, left_width);
    WINDOW *win_right = newwin(height, right_width, 0, left_width + mid_width);
    box(win_left, 0, 0); box(win_mid, 0, 0); box(win_right, 0, 0);

    char cur_path[1024];
    const char *home_path = getenv("HOME");
    if (!home_path) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home_path = pw->pw_dir;
    }
    if (!home_path) {
        endwin();
        printf("Cannot find USER directory.\n");
        return 1;
    }
    strncpy(cur_path, home_path, sizeof(cur_path));
    char *names[MAX_ITEMS]; int isdir[MAX_ITEMS], name_len[MAX_ITEMS]; int n;
    int mouse_row = -1, mouse_col = -1;
    MEVENT ev;

    while (1) { // 마우스 클릭 핸들링, 선택한 영역이 디렉터리면 새로고침 하여 하위 내용, 파일이면 일단 아무것도 하지 않음.
        werase(win_left); box(win_left, 0, 0);
        // 상단 경로/안내
        const char *display_path = cur_path;
        if (strncmp(cur_path, home_path, strlen(home_path)) == 0) {
            // 홈 디렉터리 이하일 경우 ~로 축약
            display_path = cur_path + strlen(home_path);
            if (display_path[0] == '/') display_path++;
            char temp[1024];
            snprintf(temp, sizeof(temp), "~/%s", display_path);
            display_path = temp;
        }
        mvwprintw(win_left, 1, NAME_START_COL, "Current path: %s", display_path);

        // 1. '..'(상위폴더)
        char parent_msg[] = ".. (parent dir)";
        int parent_len = strlen(parent_msg);
        wattron(win_left, COLOR_PAIR((mouse_row == LIST_START_ROW + 0 && mouse_col >= NAME_START_COL && mouse_col < NAME_START_COL + parent_len) ? 1 : 2));
        mvwprintw(win_left, LIST_START_ROW, NAME_START_COL, "%s", parent_msg);
        wattroff(win_left, COLOR_PAIR(1)); wattroff(win_left, COLOR_PAIR(2));

        // 2. 하위 목록
        n = get_dir_list(cur_path, names, isdir, name_len);
        for (int i = 0; i < n; i++) {
            int draw_row = LIST_START_ROW + 1 + i;
            int hl = (mouse_row == draw_row && mouse_col >= NAME_START_COL && mouse_col < NAME_START_COL + name_len[i]);
            wattron(win_left, COLOR_PAIR(hl ? 1 : 2));
            mvwprintw(win_left, draw_row, NAME_START_COL, "%s%s", names[i], isdir[i] ? "/" : "");
            wattroff(win_left, COLOR_PAIR(1)); wattroff(win_left, COLOR_PAIR(2));
        }
        wrefresh(win_left);

        // 가운데/오른쪽 임시 텍스트
        wattron(win_mid, COLOR_PAIR(3));
        mvwprintw(win_mid, 1, 2, "CODE/TEXT EDITOR (todo)");
        wattroff(win_mid, COLOR_PAIR(3));
        box(win_mid, 0, 0); wrefresh(win_mid);
        wattron(win_right, COLOR_PAIR(3));
        mvwprintw(win_right, 1, 2, "FUNCTIONS and CONTROL (todo)");
        wattroff(win_right, COLOR_PAIR(3));
        box(win_right, 0, 0); wrefresh(win_right);

        mvprintw(LINES-1, 0, "Quit: q");
        refresh();

        int ch = getch();
        if (ch == 'q') break;
        // 마우스 위치 갱신 및 클릭 처리
        if (ch == KEY_MOUSE) {
            if (getmouse(&ev) == OK) {
                // win_left 영역 좌표 변환
                int rel_y = ev.y, rel_x = ev.x;
                // 좌/중앙/우 윈도우 분기
                if (rel_x < left_width) {
                    mouse_row = rel_y; mouse_col = rel_x;

                    int clicked = rel_y - LIST_START_ROW;
                    int col = rel_x - NAME_START_COL;
                    if ((ev.bstate & BUTTON1_PRESSED)) {
                        // 1. '..' 클릭
                        if (clicked == 0 && col >= 0 && col < parent_len) {
                            char *lastslash = strrchr(cur_path, '/');
                            if (lastslash && lastslash != cur_path)
                                *lastslash = '\0';
                            else
                                strncpy(cur_path, home_path, sizeof(cur_path));
                        }
                        // 2. 디렉터리 클릭
                        else if (clicked >= 1 && clicked <= n && col >= 0 && col < name_len[clicked-1] && isdir[clicked-1]) {
                            if (strlen(cur_path) + strlen(names[clicked-1]) + 2 < sizeof(cur_path)) {
                                strcat(cur_path, "/");
                                strcat(cur_path, names[clicked-1]);
                            }
                        }
                    }
                }
            }
        }
        for (int i = 0; i < n; i++) free(names[i]);
    }
    delwin(win_left); delwin(win_mid); delwin(win_right); endwin();
    return 0;
}
