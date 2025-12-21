#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <ncurses.h>

// Color pairs
#define COLOR_HEADER 1
#define COLOR_SELECTED 2
#define COLOR_DIR 3
#define COLOR_FILE 4
#define COLOR_STATUSBAR 5

// Initialize UI colors and settings
void ui_init_colors(void);

// Draw a clean window with title
void ui_draw_window(WINDOW *win, const char *title);

// Draw text with proper padding to avoid border overlap
void ui_safe_print(WINDOW *win, int y, int x, const char *text);

// Get usable dimensions (excluding borders and padding)
void ui_get_usable_area(WINDOW *win, int *start_y, int *start_x, int *height, int *width);

#endif
