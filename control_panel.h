#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include <ncurses.h>

#define CP_MAX_CMD_LEN 256
#define CP_MAX_HISTORY 10
#define CP_MAX_OUTPUT_LINES 100
#define CP_MAX_OUTPUT_LEN 256

typedef enum {
    CP_MODE_NORMAL,      // Show help and options
    CP_MODE_CMD_INPUT,   // Terminal command input mode
    CP_MODE_CMD_OUTPUT,  // Show command output
} ControlPanelMode;

typedef struct {
    ControlPanelMode mode;
    char current_cmd[CP_MAX_CMD_LEN];
    int cursor_pos;

    char cmd_history[CP_MAX_HISTORY][CP_MAX_CMD_LEN];
    int history_count;
    int history_idx;

    char selected_file[1024];  // Currently selected file path
    char current_dir[1024];    // Current working directory

    // Command output
    char output_lines[CP_MAX_OUTPUT_LINES][CP_MAX_OUTPUT_LEN];
    int output_line_count;
    int output_scroll_offset;
    char last_executed_cmd[CP_MAX_CMD_LEN];
} ControlPanel;

void cp_init(ControlPanel *cp);
void cp_draw(ControlPanel *cp, WINDOW *win);
void cp_set_selected_file(ControlPanel *cp, const char *filepath);
void cp_set_current_dir(ControlPanel *cp, const char *dirpath);
void cp_set_output(ControlPanel *cp, const char *cmd, const char *output);
void cp_clear_output(ControlPanel *cp);

// Returns: 0=nothing, 1=execute vim, 2=execute command
int cp_handle_key(ControlPanel *cp, int key, char *output_cmd, size_t cmd_size);

#endif
