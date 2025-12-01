#include <ncurses.h>

int main(void) {
    initscr();
    cbreak;
    noecho();
    keypad(stdscr, TRUE);

    int h, w;
    getmaxyx(stdscr, h, w);
    mvprintw(0, 0, "size = %d x %d (q to quit)", h, w);
    refresh();

    int ch;
    while ((ch = getch()) != 'q') {
        if (ch == KEY_RESIZE) {
            getmaxyx(stdscr, h, w);
            clear();
            mvprintw(0, 0, "size = %d x %d (q to quit)", h, w);
            refresh();
        } else {
            mvprintw(1, 0, "last key: %d ", ch);
            refresh();
        }
    }

    endwin();
    return 0;
}