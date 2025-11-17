#include <ncurses.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define MAX_ITEMS 1024
#define LIST_START_ROW 3      // 목록 시작 줄(테두리, 안내 등 고려)
#define NAME_START_COL 2      // 목록 항목 왼쪽 시작 열

int not_hidden(const struct dirent *e) { return e->d_name[0] != '.'; }

int get_dir_list(const char *path, char **names, int *isdir) {
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
        free(namelist[i]);
        count++;
    }
    free(namelist);
    return count;
}

int main() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    mousemask(ALL_MOUSE_EVENTS, NULL);
    mouseinterval(0); // 클릭 반응 최대한 빠르게
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_CYAN); // 하이라이트 색상
    init_pair(2, COLOR_WHITE, COLOR_BLACK); // 기본 색상
    char cur_path[1024];
    const char *home = getenv("HOME");
    strncpy(cur_path, home, sizeof(cur_path));
    char *names[MAX_ITEMS]; int isdir[MAX_ITEMS], name_len[MAX_ITEMS]; int n;
    int run = 1;
    // 마우스 위치 추적 용
    int mouse_row = -1, mouse_col = -1;
    MEVENT ev;
    while (run) {
        clear();
        mvprintw(0, 0, "Current path: %s", cur_path);
        mvprintw(1, NAME_START_COL, "Use mouse to select directory. \"..\" is parent directory.");
        mvprintw(LIST_START_ROW, NAME_START_COL, ".. (parent dir)");
        n = get_dir_list(cur_path, names, isdir);
        // 각 항목별 실제 화면에 표시될 너비 미리 기록
        name_len[0] = 14; // ".. (parent dir)"
        for (int i = 0; i < n; i++) {
            int l = strlen(names[i]) + (isdir[i] ? 1 : 0); // "/" 붙임
            name_len[i+1] = l;
        }
        // 목록 반복: hover 효과 (마우스 위치 항목 색상 변경)
        for (int i = 0; i < n+1; i++) {
            int this_row = LIST_START_ROW + i;
            attr_t set_attr = COLOR_PAIR(2);
            // 마우스가 이 행/항목 너비 안에 있으면 강조
            if (mouse_row == this_row && mouse_col >= NAME_START_COL && mouse_col < NAME_START_COL + name_len[i])
                set_attr = COLOR_PAIR(1) | A_BOLD;
            attron(set_attr);
            if (i == 0)
                mvprintw(this_row, NAME_START_COL, ".. (parent dir)");
            else
                mvprintw(this_row, NAME_START_COL, "%s%s", names[i-1], isdir[i-1] ? "/" : "");
            attroff(set_attr);
        }
        mvprintw(LINES-1, 0, "q: Quit | Mouse: select entry");
        refresh();

        int ch = getch();
        if (ch == 'q') break;
        // 마우스 위치 추적 및 클릭 처리
        if (ch == KEY_MOUSE) {
            if (getmouse(&ev) == OK) {
                mouse_row = ev.y;
                mouse_col = ev.x;
                int clicked = ev.y - LIST_START_ROW;    // 실제 선택 인덱스
                int col = ev.x - NAME_START_COL;        // 항목 내 상대 위치
                // 클릭이 목록 내, 이름 너비 내에서만 인정
                if ((ev.bstate & BUTTON1_PRESSED) && clicked >= 0 && clicked <= n &&
                    col >= 0 && col < name_len[clicked]) {
                    if (clicked == 0) {
                        char *lastslash = strrchr(cur_path, '/');
                        if (lastslash && lastslash != cur_path)
                            *lastslash = '\0';
                        else
                            strcpy(cur_path, home);
                    } else if (clicked >= 1 && clicked <= n && isdir[clicked-1]) {
                        if (strlen(cur_path) + strlen(names[clicked-1]) + 2 < sizeof(cur_path)) {
                            strcat(cur_path, "/");
                            strcat(cur_path, names[clicked-1]);
                        }
                    }
                }
                // 클릭이 아니면 hover만 반영
            }
        }
        for (int i = 0; i < n; i++) free(names[i]);
    }
    endwin();
    return 0;
}
