#ifndef CODE_VIEW_H
#define CODE_VIEW_H

#include <ncurses.h>

#define CV_MAX_LINES 2000
#define CV_MAX_LINELEN 256
#define CV_MAX_VLINES (CV_MAX_LINES * 8)

typedef struct {
    char filename[256];
    char lines[CV_MAX_LINES][CV_MAX_LINELEN];
    int line_count;

    int gutter_width;
    int view_start;
    int visible_height;

    struct {
        int src_line;
        int wrap_idx;
    } vlines[CV_MAX_VLINES];
    int vline_count;

} CodeView;

void cv_init(CodeView* cv);
void cv_load(CodeView* cv, const char* path);
void rebuild_layout(CodeView* cv, int term_width);
void cv_draw(CodeView* cv, WINDOW* win);
void cv_handle_key(CodeView* cv, int key);

#endif