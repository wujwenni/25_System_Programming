#include "code_view.h"
#include "ui_helpers.h"
#include <stdio.h>
#include <string.h>

void cv_init(CodeView *cv) {
    cv->filename[0]=0;
    cv->line_count=0;
}
void cv_load(CodeView* cv, const char* path) {
    strncpy(cv->filename, path, sizeof(cv->filename)-1);
    cv->filename[sizeof(cv->filename)-1]=0;
    cv->line_count=0;
    FILE* f=fopen(path,"r");
    if(!f) return;
    while(cv->line_count<CV_MAX_LINES && 
          fgets(cv->lines[cv->line_count],CV_MAX_LINELEN,f)) {
            cv->lines[cv->line_count][strcspn(cv->lines[cv->line_count],"\n")]=0;
            cv->line_count++;
          }
    fclose(f);
}

void cv_rebuild_layout(CodeView* cv, int term_width) {
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

    char title[512];
    if (cv->filename[0]) {
        // Extract just the filename from path
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

    // Draw file content
    wattron(win, COLOR_PAIR(COLOR_FILE));
    for(int i = 0; i < cv->line_count && i < height; i++) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s", cv->lines[i]);
        ui_safe_print(win, start_y + i, start_x, buffer);
    }
    wattroff(win, COLOR_PAIR(COLOR_FILE));

    // Show indicator if there are more lines
    if (cv->line_count > height) {
        wattron(win, COLOR_PAIR(COLOR_STATUSBAR));
        char more[32];
        snprintf(more, sizeof(more), "... +%d lines", cv->line_count - height);
        ui_safe_print(win, start_y + height - 1, start_x, more);
        wattroff(win, COLOR_PAIR(COLOR_STATUSBAR));
    }
}
