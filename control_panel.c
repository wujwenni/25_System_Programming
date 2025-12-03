#include "control_panel.h"
#include "ui_helpers.h"
#include <string.h>
#include <ctype.h>

void cp_init(ControlPanel *cp) {
    cp->mode = CP_MODE_NORMAL;
    cp->current_cmd[0] = '\0';
    cp->cursor_pos = 0;
    cp->history_count = 0;
    cp->history_idx = -1;
    cp->selected_file[0] = '\0';
    cp->output_line_count = 0;
    cp->output_scroll_offset = 0;
    cp->last_executed_cmd[0] = '\0';
}

void cp_clear_output(ControlPanel *cp) {
    cp->output_line_count = 0;
    cp->output_scroll_offset = 0;
    cp->last_executed_cmd[0] = '\0';
}

void cp_set_output(ControlPanel *cp, const char *cmd, const char *output) {
    cp->output_line_count = 0;
    cp->output_scroll_offset = 0;
    strncpy(cp->last_executed_cmd, cmd, sizeof(cp->last_executed_cmd) - 1);
    cp->last_executed_cmd[sizeof(cp->last_executed_cmd) - 1] = '\0';

    if (!output) return;

    // Split output into lines
    const char *line_start = output;
    const char *line_end;

    while (*line_start && cp->output_line_count < CP_MAX_OUTPUT_LINES) {
        line_end = strchr(line_start, '\n');
        if (line_end) {
            int len = line_end - line_start;
            if (len > CP_MAX_OUTPUT_LEN - 1) len = CP_MAX_OUTPUT_LEN - 1;
            strncpy(cp->output_lines[cp->output_line_count], line_start, len);
            cp->output_lines[cp->output_line_count][len] = '\0';
            line_start = line_end + 1;
        } else {
            strncpy(cp->output_lines[cp->output_line_count], line_start,
                   CP_MAX_OUTPUT_LEN - 1);
            cp->output_lines[cp->output_line_count][CP_MAX_OUTPUT_LEN - 1] = '\0';
            break;
        }
        cp->output_line_count++;
    }

    if (cp->output_line_count > 0) {
        cp->mode = CP_MODE_CMD_OUTPUT;
    }
}

void cp_set_selected_file(ControlPanel *cp, const char *filepath) {
    if (filepath) {
        strncpy(cp->selected_file, filepath, sizeof(cp->selected_file) - 1);
        cp->selected_file[sizeof(cp->selected_file) - 1] = '\0';
    } else {
        cp->selected_file[0] = '\0';
    }
}

static int is_text_file(const char *filepath) {
    if (!filepath || !filepath[0]) return 0;

    const char *ext = strrchr(filepath, '.');
    if (!ext) return 0;

    // Check common text/source file extensions
    const char *text_exts[] = {
        ".txt", ".c", ".h", ".cpp", ".hpp", ".py", ".java", ".js",
        ".html", ".css", ".md", ".json", ".xml", ".sh", ".makefile",
        ".yaml", ".yml", ".conf", ".cfg", ".ini", NULL
    };

    for (int i = 0; text_exts[i]; i++) {
        if (strcasecmp(ext, text_exts[i]) == 0) {
            return 1;
        }
    }

    // Check if filename is "Makefile"
    const char *basename = strrchr(filepath, '/');
    basename = basename ? basename + 1 : filepath;
    if (strcasecmp(basename, "Makefile") == 0) {
        return 1;
    }

    return 0;
}

void cp_draw(ControlPanel *cp, WINDOW *win) {
    int start_y, start_x, height, width;
    ui_get_usable_area(win, &start_y, &start_x, &height, &width);

    if (cp->mode == CP_MODE_CMD_OUTPUT) {
        // Command output mode
        ui_draw_window(win, "OUTPUT");

        wattron(win, COLOR_PAIR(COLOR_HEADER));
        char header[512];
        snprintf(header, sizeof(header), "Command: %s", cp->last_executed_cmd);
        ui_safe_print(win, start_y, start_x, header);
        wattroff(win, COLOR_PAIR(COLOR_HEADER));

        wattron(win, COLOR_PAIR(COLOR_STATUSBAR) | A_DIM);
        ui_safe_print(win, start_y + 1, start_x, "(ESC: back, Up, Down: scroll)");
        wattroff(win, COLOR_PAIR(COLOR_STATUSBAR) | A_DIM);

        // Draw output lines
        wattron(win, COLOR_PAIR(COLOR_FILE));
        int display_height = height - 3;
        for (int i = 0; i < display_height && (cp->output_scroll_offset + i) < cp->output_line_count; i++) {
            ui_safe_print(win, start_y + 3 + i, start_x,
                         cp->output_lines[cp->output_scroll_offset + i]);
        }
        wattroff(win, COLOR_PAIR(COLOR_FILE));

        // Show scroll indicator
        if (cp->output_line_count > display_height) {
            wattron(win, COLOR_PAIR(COLOR_STATUSBAR));
            char scroll_info[64];
            snprintf(scroll_info, sizeof(scroll_info), "[%d/%d lines]",
                    cp->output_scroll_offset + 1, cp->output_line_count);
            ui_safe_print(win, start_y + height - 1, start_x, scroll_info);
            wattroff(win, COLOR_PAIR(COLOR_STATUSBAR));
        }
    } else if (cp->mode == CP_MODE_CMD_INPUT) {
        // Command input mode
        ui_draw_window(win, "TERMINAL");

        wattron(win, COLOR_PAIR(COLOR_STATUSBAR));
        ui_safe_print(win, start_y, start_x, "Enter command (ESC to cancel):");
        wattroff(win, COLOR_PAIR(COLOR_STATUSBAR));

        wattron(win, COLOR_PAIR(COLOR_FILE));
        char prompt[512];
        snprintf(prompt, sizeof(prompt), "$ %s", cp->current_cmd);
        ui_safe_print(win, start_y + 2, start_x, prompt);
        wattroff(win, COLOR_PAIR(COLOR_FILE));

        // Show command history
        if (cp->history_count > 0) {
            wattron(win, COLOR_PAIR(COLOR_FILE) | A_DIM);
            ui_safe_print(win, start_y + 4, start_x, "History:");
            int y = start_y + 5;
            for (int i = cp->history_count - 1; i >= 0 && y < start_y + height - 1; i--) {
                char hist[512];
                snprintf(hist, sizeof(hist), "  %s", cp->cmd_history[i]);
                ui_safe_print(win, y++, start_x, hist);
            }
            wattroff(win, COLOR_PAIR(COLOR_FILE) | A_DIM);
        }
    } else {
        // Normal mode - show actions and help
        ui_draw_window(win, "ACTIONS");

        int y = start_y;

        // File actions section
        wattron(win, COLOR_PAIR(COLOR_HEADER));
        ui_safe_print(win, y++, start_x, "File Actions:");
        wattroff(win, COLOR_PAIR(COLOR_HEADER));
        y++;

        if (cp->selected_file[0] && is_text_file(cp->selected_file)) {
            wattron(win, COLOR_PAIR(COLOR_FILE));
            ui_safe_print(win, y++, start_x, "  v - Open in vim");
            wattroff(win, COLOR_PAIR(COLOR_FILE));
        } else {
            wattron(win, COLOR_PAIR(COLOR_FILE) | A_DIM);
            ui_safe_print(win, y++, start_x, "  (Select text file)");
            wattroff(win, COLOR_PAIR(COLOR_FILE) | A_DIM);
        }
        y++;

        // Terminal section
        wattron(win, COLOR_PAIR(COLOR_HEADER));
        ui_safe_print(win, y++, start_x, "Terminal:");
        wattroff(win, COLOR_PAIR(COLOR_HEADER));
        y++;

        wattron(win, COLOR_PAIR(COLOR_FILE));
        ui_safe_print(win, y++, start_x, "  : - Command mode");
        wattroff(win, COLOR_PAIR(COLOR_FILE));
        y++;

        // Navigation help section
        wattron(win, COLOR_PAIR(COLOR_HEADER));
        ui_safe_print(win, y++, start_x, "Navigation:");
        wattroff(win, COLOR_PAIR(COLOR_HEADER));
        y++;

        wattron(win, COLOR_PAIR(COLOR_FILE));
        ui_safe_print(win, y++, start_x, "  Up/Down - Move");
        ui_safe_print(win, y++, start_x, "  left   - Parent");
        ui_safe_print(win, y++, start_x, "  right/Enter - Select");
        y++;
        ui_safe_print(win, y++, start_x, "  q   - Quit");
        wattroff(win, COLOR_PAIR(COLOR_FILE));

        // Show selected file info
        if (cp->selected_file[0]) {
            y = start_y + height - 3;
            wattron(win, COLOR_PAIR(COLOR_STATUSBAR));
            const char *basename = strrchr(cp->selected_file, '/');
            basename = basename ? basename + 1 : cp->selected_file;
            char info[256];
            snprintf(info, sizeof(info), "Selected: %.30s", basename);
            ui_safe_print(win, y, start_x, info);
            wattroff(win, COLOR_PAIR(COLOR_STATUSBAR));
        }
    }
}

int cp_handle_key(ControlPanel *cp, int key, char *output_cmd, size_t cmd_size) {
    if (cp->mode == CP_MODE_CMD_OUTPUT) {
        // Output viewing mode
        if (key == 27) {  // ESC
            cp->mode = CP_MODE_NORMAL;
            return 0;
        } else if (key == KEY_UP) {
            if (cp->output_scroll_offset > 0) {
                cp->output_scroll_offset--;
            }
            return 0;
        } else if (key == KEY_DOWN) {
            if (cp->output_scroll_offset < cp->output_line_count - 1) {
                cp->output_scroll_offset++;
            }
            return 0;
        }
        return 0;
    } else if (cp->mode == CP_MODE_CMD_INPUT) {
        // Command input mode
        if (key == 27) {  // ESC
            cp->mode = CP_MODE_NORMAL;
            cp->current_cmd[0] = '\0';
            cp->cursor_pos = 0;
            return 0;
        } else if (key == '\n' || key == KEY_ENTER) {
            // Execute command
            if (cp->current_cmd[0]) {
                strncpy(output_cmd, cp->current_cmd, cmd_size - 1);
                output_cmd[cmd_size - 1] = '\0';

                // Add to history
                if (cp->history_count < CP_MAX_HISTORY) {
                    strncpy(cp->cmd_history[cp->history_count],
                           cp->current_cmd, CP_MAX_CMD_LEN - 1);
                    cp->cmd_history[cp->history_count][CP_MAX_CMD_LEN - 1] = '\0';
                    cp->history_count++;
                } else {
                    // Shift history
                    for (int i = 0; i < CP_MAX_HISTORY - 1; i++) {
                        strncpy(cp->cmd_history[i], cp->cmd_history[i + 1],
                               CP_MAX_CMD_LEN);
                    }
                    strncpy(cp->cmd_history[CP_MAX_HISTORY - 1],
                           cp->current_cmd, CP_MAX_CMD_LEN - 1);
                }

                cp->mode = CP_MODE_NORMAL;
                cp->current_cmd[0] = '\0';
                cp->cursor_pos = 0;
                return 2;  // Execute command
            }
            return 0;
        } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            // Backspace
            if (cp->cursor_pos > 0) {
                cp->cursor_pos--;
                cp->current_cmd[cp->cursor_pos] = '\0';
            }
            return 0;
        } else if (isprint(key) && cp->cursor_pos < CP_MAX_CMD_LEN - 1) {
            // Add character
            cp->current_cmd[cp->cursor_pos++] = key;
            cp->current_cmd[cp->cursor_pos] = '\0';
            return 0;
        }
        return 0;
    } else {
        // Normal mode
        if (key == ':') {
            // Enter command mode
            cp->mode = CP_MODE_CMD_INPUT;
            cp->current_cmd[0] = '\0';
            cp->cursor_pos = 0;
            return 0;
        } else if (key == 'v' || key == 'V') {
            // Open in vim
            if (cp->selected_file[0] && is_text_file(cp->selected_file)) {
                snprintf(output_cmd, cmd_size, "vim %s", cp->selected_file);
                return 1;  // Execute vim
            }
            return 0;
        }
        return 0;
    }
}
