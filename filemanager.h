#ifndef FILEMAMAGER_H
#define FILEMAMAGER_H

#include <ncurses.h>

#define FM_MAX_ITEMS 1024

typedef struct {
    char cur_path[1024];
    char* names[FM_MAX_ITEMS];
    int isdir[FM_MAX_ITEMS];
    int name_len[FM_MAX_ITEMS];
    int count;

    int selected_idx;  // -1 = "..", 0+ = file index
    int scroll_offset;
    int visible_height;  // Track visible area height
} FileManager;

void fm_init(FileManager* fm, const char* home_path);
void fm_refresh_list(FileManager* fm);
void fm_draw(FileManager* fm, WINDOW* win);

int fm_handle_mouse(FileManager* fm, MEVENT* ev, int left_width, char* selected_path, size_t sel_size);
int fm_handle_key(FileManager* fm, int key, char* selected_path, size_t sel_size);

void fm_cleanup(FileManager* fm);

#endif