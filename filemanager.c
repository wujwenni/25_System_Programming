#include "filemanager.h"
#include "ui_helpers.h"
#include <dirent.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LIST_START_ROW 2
#define NAME_START_COL 2

int not_hidden(const struct dirent *e) {
    return e->d_name[0] != '.';
}

void fm_init(FileManager *fm, const char *home_dir) {
    strncpy(fm->cur_path, home_dir, 1023);
    fm->cur_path[1023] = '\0';
    fm->count = 0;
    fm->selected_idx = -1;  // Start with ".." selected
    fm->scroll_offset = 0;
    fm->visible_height = 20;  // Default, will be updated in fm_draw
}

void fm_cleanup(FileManager *fm) {
    for (int i = 0; i < fm->count; i++) {
        free(fm->names[i]);
    }
    fm->count = 0;
}

void fm_refresh_list(FileManager *fm) {
    fm_cleanup(fm);
    struct dirent **namelist;
    int n = scandir(fm->cur_path, &namelist, not_hidden, alphasort);  
    fm->count = 0;
    for (int i = 0; i < n && fm->count < FM_MAX_ITEMS-1; i++) {
        fm->names[fm->count] = strdup(namelist[i]->d_name);
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", fm->cur_path, namelist[i]->d_name);
        struct stat st;
        if (namelist[i]->d_type == DT_UNKNOWN) {
            fm->isdir[fm->count] = (!stat(path, &st) && S_ISDIR(st.st_mode));
        } else {
            fm->isdir[fm->count] = (namelist[i]->d_type == DT_DIR);
        }
        fm->name_len[fm->count] = strlen(namelist[i]->d_name) + (fm->isdir[fm->count] ? 1 : 0);
        free(namelist[i]);
        fm->count++;
    }
    if (namelist) free(namelist);
}

void fm_draw(FileManager *fm, WINDOW *win) {
    int start_y, start_x, height, width;
    ui_get_usable_area(win, &start_y, &start_x, &height, &width);

    fm->visible_height = height - 1;

    ui_draw_window(win, "FILE BROWSER");

    wattron(win, COLOR_PAIR(COLOR_HEADER));
    ui_safe_print(win, 1, 2, fm->cur_path);
    wattroff(win, COLOR_PAIR(COLOR_HEADER));
    int y = start_y;
    if (fm->selected_idx == -1) {
        wattron(win, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        ui_safe_print(win, y, start_x, " .. ");
        wattroff(win, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
    } else {
        wattron(win, COLOR_PAIR(COLOR_DIR));
        ui_safe_print(win, y, start_x, " .. ");
        wattroff(win, COLOR_PAIR(COLOR_DIR));
    }
    y++;

    for (int i = fm->scroll_offset; i < fm->count && (y - start_y) < height; i++) {
        int is_selected = (fm->selected_idx == i);

        if (is_selected) {
            wattron(win, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        } else if (fm->isdir[i]) {
            wattron(win, COLOR_PAIR(COLOR_DIR));
        } else {
            wattron(win, COLOR_PAIR(COLOR_FILE));
        }

        char buffer[256];
        snprintf(buffer, sizeof(buffer), " %s%s",
                 fm->names[i], fm->isdir[i] ? "/" : "");
        ui_safe_print(win, y, start_x, buffer);

        if (is_selected) {
            wattroff(win, COLOR_PAIR(COLOR_SELECTED) | A_BOLD);
        } else if (fm->isdir[i]) {
            wattroff(win, COLOR_PAIR(COLOR_DIR));
        } else {
            wattroff(win, COLOR_PAIR(COLOR_FILE));
        }
        y++;
    }

}


int fm_handle_mouse(FileManager* fm, MEVENT* ev,
                    int left_width, char* selected_path, size_t sel_size) {
    if (!(ev->bstate & BUTTON1_PRESSED)) return 0;
    if (ev->x >= 0 && ev->x < left_width) {
        int row = ev->y;
        int clicked = row - LIST_START_ROW;
        if (clicked == 0) {
            fm->selected_idx = -1;
            char tmp[1024];
            strncpy(tmp, fm->cur_path, 1023);
            tmp[1023] = '\0';
            char *last = strrchr(tmp, '/');
            if (last && last != tmp) *last = '\0';
            strncpy(selected_path, tmp, sel_size);
            selected_path[sel_size-1] = '\0';
            return 1;
        }
        else if (clicked > 0 && clicked <= fm->count) {
            int idx = clicked - 1;
            fm->selected_idx = idx;
            snprintf(selected_path, sel_size, "%s/%s",
                     fm->cur_path, fm->names[idx]);
            return fm->isdir[idx] ? 1 : 2;
        }
    }
    return 0;
}

int fm_handle_key(FileManager* fm, int key, char* selected_path, size_t sel_size) {
    switch (key) {
        case KEY_UP:
            if (fm->selected_idx > -1) {
                fm->selected_idx--;
                if (fm->selected_idx >= 0 && fm->selected_idx < fm->scroll_offset) {
                    fm->scroll_offset = fm->selected_idx;
                }
            }
            return 0;

        case KEY_DOWN:
            if (fm->selected_idx < fm->count - 1) {
                fm->selected_idx++;
                if (fm->selected_idx >= fm->scroll_offset + fm->visible_height) {
                    fm->scroll_offset = fm->selected_idx - fm->visible_height + 1;
                }
            }
            return 0;

        case '\n':
        case KEY_ENTER:
        case KEY_RIGHT:
            if (fm->selected_idx == -1) {
                char tmp[1024];
                strncpy(tmp, fm->cur_path, sizeof(tmp)-1);
                tmp[sizeof(tmp)-1] = '\0';
                char *last = strrchr(tmp, '/');
                if (last && last != tmp) *last = '\0';
                strncpy(selected_path, tmp, sel_size);
                selected_path[sel_size-1] = '\0';
                return 1;
            } else if (fm->selected_idx >= 0 && fm->selected_idx < fm->count) {
                snprintf(selected_path, sel_size, "%s/%s",
                         fm->cur_path, fm->names[fm->selected_idx]);
                return fm->isdir[fm->selected_idx] ? 1 : 2;
            }
            return 0;

        case KEY_LEFT:
            char tmp[1024];
            strncpy(tmp, fm->cur_path, sizeof(tmp)-1);
            tmp[sizeof(tmp)-1] = '\0';
            char *last = strrchr(tmp, '/');
            if (last && last != tmp) *last = '\0';
            strncpy(selected_path, tmp, sel_size);
            selected_path[sel_size-1] = '\0';
            fm->selected_idx = -1;
            return 1;
    }
    return 0;
}

