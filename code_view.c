#include "code_view.h"
#include "ui_helpers.h"
#include <stdio.h>
#include <string.h>

void cv_init(CodeView *cv) {
    cv->filename[0]=0;
    cv->line_count=0;
    cv->view_start=0;
    cv->visible_height=20;
}
void cv_load(CodeView* cv, const char* path) {
    strncpy(cv->filename, path, sizeof(cv->filename)-1);
    cv->filename[sizeof(cv->filename)-1]=0;
    cv->line_count=0;
    cv->view_start=0;
    FILE* f=fopen(path,"r");
    if(!f) return;
    while(cv->line_count<CV_MAX_LINES &&
          fgets(cv->lines[cv->line_count],CV_MAX_LINELEN,f)) {
            cv->lines[cv->line_count][strcspn(cv->lines[cv->line_count],"\n")]=0;
            cv->line_count++;
          }
    fclose(f);
}

void rebuild_layout(CodeView* cv, int term_width) {
    cv->vline_count = 0;
    int code_start_x = cv->gutter_width;
    int code_width = term_width - code_start_x - 1;
    if (code_width < 1) code_width = 1;
    for (int i = 0; i < cv->line_count; i++) {
        int len = (int)strlen(cv->lines[i]);
        int needed = (len == 0) ? 1 : ( (len + code_width - 1) / code_width );
        for (int w = 0; w < needed && cv->vline_count < CV_MAX_VLINES; w++) {
            cv->vlines[cv->vline_count].src_line = i;
            cv->vlines[cv->vline_count].wrap_idx = w;
            cv->vline_count++;
        }
    }
}

void cv_draw(CodeView* cv, WINDOW* win) {
    int start_y, start_x, height, width;
    ui_get_usable_area(win, &start_y, &start_x, &height, &width);

    cv->visible_height = height;

    char title[512];
    if (cv->filename[0]) {
        const char *basename = strrchr(cv->filename, '/');
        basename = basename ? basename + 1 : cv->filename;
        snprintf(title, sizeof(title), "PREVIEW: %.200s", basename);
    } else {
        snprintf(title, sizeof(title), "PREVIEW");
    }
    ui_draw_window(win, title);

    if (!cv->filename[0]) {
        wattron(win, COLOR_PAIR(COLOR_FILE) | A_DIM);
        ui_safe_print(win, start_y + height/2, start_x, "Select a file to preview...");
        wattroff(win, COLOR_PAIR(COLOR_FILE) | A_DIM);
        return;
    }

    wattron(win, COLOR_PAIR(COLOR_FILE));
    int line_idx = cv->view_start;
    for(int y = 0; y < height && line_idx < cv->line_count; y++, line_idx++) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s", cv->lines[line_idx]);
        ui_safe_print(win, start_y + y, start_x, buffer);
    }
    wattroff(win, COLOR_PAIR(COLOR_FILE));

    if (cv->line_count > height) {
        int current_page = (cv->view_start / cv->visible_height) + 1;
        int total_pages = (cv->line_count + cv->visible_height - 1) / cv->visible_height;

        char page_info[64];
        snprintf(page_info, sizeof(page_info), " Page %d/%d ",
                current_page, total_pages);

        int info_y = getmaxy(win) - 1;
        int info_x = getmaxx(win) - strlen(page_info) - 1;

        wattron(win, COLOR_PAIR(COLOR_HEADER) | A_DIM);
        mvwprintw(win, info_y, info_x, "%s", page_info);
        wattroff(win, COLOR_PAIR(COLOR_HEADER) | A_DIM);
    }
}

void cv_handle_key(CodeView* cv, int key) {
    switch (key) {
        case KEY_UP:
            if (cv->view_start > 0) {
                cv->view_start--;
            }
            break;

        case KEY_DOWN:
            if (cv->view_start + cv->visible_height < cv->line_count) {
                cv->view_start++;
            }
            break;

        case KEY_PPAGE:
            cv->view_start -= cv->visible_height;
            if (cv->view_start < 0) {
                cv->view_start = 0;
            }
            break;

        case KEY_NPAGE:
            cv->view_start += cv->visible_height;
            if (cv->view_start + cv->visible_height >= cv->line_count) {
                cv->view_start = cv->line_count - cv->visible_height;
                if (cv->view_start < 0) cv->view_start = 0;
            }
            break;

        case KEY_HOME:
            cv->view_start = 0;
            break;

        case KEY_END:
            cv->view_start = cv->line_count - cv->visible_height;
            if (cv->view_start < 0) cv->view_start = 0;
            break;
    }
}
