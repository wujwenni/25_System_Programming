#include "ui_helpers.h"
#include <string.h>

void ui_init_colors(void) {
    start_color();
    init_pair(COLOR_HEADER, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_SELECTED, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_DIR, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_FILE, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_STATUSBAR, COLOR_BLACK, COLOR_YELLOW);
}

void ui_draw_window(WINDOW *win, const char *title) {
    werase(win);
    box(win, 0, 0);

    if (title && title[0]) {
        wattron(win, COLOR_PAIR(COLOR_HEADER) | A_BOLD);
        int max_x = getmaxx(win);
        int title_len = strlen(title);
        int start_x = (max_x - title_len - 4) / 2;
        if (start_x < 1) start_x = 1;
        mvwprintw(win, 0, start_x, " %s ", title);
        wattroff(win, COLOR_PAIR(COLOR_HEADER) | A_BOLD);
    }
}

void ui_safe_print(WINDOW *win, int y, int x, const char *text) {
    int max_y = getmaxy(win);
    int max_x = getmaxx(win);

    if (y <= 0 || y >= max_y - 1) return;
    if (x <= 0 || x >= max_x - 1) return;

    int available = max_x - x - 1;
    if (available <= 0) return;

    char buffer[1024];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    if ((int)strlen(buffer) > available) {
        buffer[available - 3] = '.';
        buffer[available - 2] = '.';
        buffer[available - 1] = '.';
        buffer[available] = '\0';
    }

    mvwprintw(win, y, x, "%s", buffer);
}

void ui_get_usable_area(WINDOW *win, int *start_y, int *start_x, int *height, int *width) {
    int max_y = getmaxy(win);
    int max_x = getmaxx(win);

    *start_y = 2;
    *start_x = 2;
    *height = max_y - 3;
    *width = max_x - 3;

    if (*height < 1) *height = 1;
    if (*width < 1) *width = 1;
}
