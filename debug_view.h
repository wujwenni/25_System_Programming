#ifndef DEBUG_VIEW_H
#define DEBUG_VIEW_H

#include <ncurses.h>
#include "debugger.h"

typedef struct {
    Debugger debugger;
    char source_lines[2000][256];
    int source_line_count;
    int scroll_offset;
    int source_loaded;
} DebugView;

void dv_init(DebugView *dv);
int dv_load_program(DebugView *dv, const char *executable_path, const char *source_path);
void dv_draw(DebugView *dv, WINDOW *win_code, WINDOW *win_output, WINDOW *win_info);

// Returns: 0=nothing, 1=exit debug mode, 2=program exited
int dv_handle_key(DebugView *dv, int key);

#endif
