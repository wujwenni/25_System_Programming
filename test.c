#include <ncurses.h>
#include <stdlib.h>

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, NULL); // 마우스 이벤트 활성화
    curs_set(0);

    mvprintw(0, 0, "Left click: BUTTON1_PRESSED | Press q to quit");
    refresh();

    while (1) {
        int ch = getch();
        if (ch == 'q') break;
        if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                clear();
                mvprintw(0, 0, "Mouse event detected!");
                mvprintw(1, 0, "x=%d, y=%d", event.x, event.y);
                mvprintw(2, 0, "bstate=0x%x", event.bstate);
                if (event.bstate & BUTTON1_PRESSED)
                    mvprintw(3, 0, "LEFT BUTTON1_PRESSED");
                if (event.bstate & BUTTON2_PRESSED)
                    mvprintw(4, 0, "RIGHT BUTTON2_PRESSED");
                if (event.bstate & BUTTON3_PRESSED)
                    mvprintw(5, 0, "MIDDLE BUTTON3_PRESSED");
                mvprintw(7, 0, "Press q to quit");
                refresh();
            }
        } else {
            mvprintw(10, 0, "Key pressed: %d", ch);
            refresh();
        }
    }
    endwin();
    return 0;
}
