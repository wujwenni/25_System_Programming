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
    cp->current_dir[0] = '\0';
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
    strncpy(cp->last_executed_cmd, cmd, 255);
    cp->last_executed_cmd[255] = '\0';

    if (!output) return;

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

void cp_set_current_dir(ControlPanel *cp, const char *dirpath) {
    if (dirpath) {
        strncpy(cp->current_dir, dirpath, sizeof(cp->current_dir) - 1);
        cp->current_dir[sizeof(cp->current_dir) - 1] = '\0';
    } else {
        cp->current_dir[0] = '\0';
    }
}

static int is_text_file(const char *filepath) {
    if (!filepath || !filepath[0]) return 0;

    const char *ext = strrchr(filepath, '.');
    if (!ext) return 0;

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
        ui_draw_window(win, "OUTPUT");

        wattron(win, COLOR_PAIR(COLOR_HEADER));
        char header[512];
        snprintf(header, sizeof(header), "Command: %s", cp->last_executed_cmd);
        ui_safe_print(win, start_y, start_x, header);
        wattroff(win, COLOR_PAIR(COLOR_HEADER));

        wattron(win, COLOR_PAIR(COLOR_STATUSBAR) | A_DIM);
        ui_safe_print(win, start_y + 1, start_x, "(ESC: back, Up, Down: scroll)");
        wattroff(win, COLOR_PAIR(COLOR_STATUSBAR) | A_DIM);

        wattron(win, COLOR_PAIR(COLOR_FILE));
        int display_height = height - 3;
        for (int i = 0; i < display_height && (cp->output_scroll_offset + i) < cp->output_line_count; i++) {
            ui_safe_print(win, start_y + 3 + i, start_x,
                         cp->output_lines[cp->output_scroll_offset + i]);
        }
        wattroff(win, COLOR_PAIR(COLOR_FILE));

        if (cp->output_line_count > display_height) {
            wattron(win, COLOR_PAIR(COLOR_STATUSBAR));
            char scroll_info[64];
            snprintf(scroll_info, sizeof(scroll_info), "[%d/%d lines]",
                    cp->output_scroll_offset + 1, cp->output_line_count);
            ui_safe_print(win, start_y + height - 1, start_x, scroll_info);
            wattroff(win, COLOR_PAIR(COLOR_STATUSBAR));
        }
    } else if (cp->mode == CP_MODE_CMD_INPUT) {
        ui_draw_window(win, "TERMINAL");

        wattron(win, COLOR_PAIR(COLOR_STATUSBAR));
        ui_safe_print(win, start_y, start_x, "Enter command (ESC to cancel):");
        wattroff(win, COLOR_PAIR(COLOR_STATUSBAR));

        wattron(win, COLOR_PAIR(COLOR_FILE));
        char prompt[512];
        snprintf(prompt, sizeof(prompt), "$ %s", cp->current_cmd);
        ui_safe_print(win, start_y + 2, start_x, prompt);
        wattroff(win, COLOR_PAIR(COLOR_FILE));

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
        ui_draw_window(win, "ACTIONS");

        int y = start_y;

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

        wattron(win, COLOR_PAIR(COLOR_HEADER));
        ui_safe_print(win, y++, start_x, "Terminal:");
        wattroff(win, COLOR_PAIR(COLOR_HEADER));
        y++;

        wattron(win, COLOR_PAIR(COLOR_FILE));
        ui_safe_print(win, y++, start_x, "  : - Command mode");
        wattroff(win, COLOR_PAIR(COLOR_FILE));
        y++;

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
        if (key == 27) {
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
        if (key == 27) {
            cp->mode = CP_MODE_NORMAL;
            cp->current_cmd[0] = '\0';
            cp->cursor_pos = 0;
            cp->history_idx = -1;
            return 0;
        } else if (key == '\n' || key == KEY_ENTER) {
            if (cp->current_cmd[0]) {
                strncpy(output_cmd, cp->current_cmd, cmd_size - 1);
                output_cmd[cmd_size - 1] = '\0';

                if (cp->history_count < CP_MAX_HISTORY) {
                    strncpy(cp->cmd_history[cp->history_count],
                           cp->current_cmd, CP_MAX_CMD_LEN - 1);
                    cp->cmd_history[cp->history_count][CP_MAX_CMD_LEN - 1] = '\0';
                    cp->history_count++;
                } else {
                    for (int i = 0; i < CP_MAX_HISTORY - 1; i++) {
                        strcpy(cp->cmd_history[i], cp->cmd_history[i + 1]);
                    }
                    strcpy(cp->cmd_history[CP_MAX_HISTORY - 1], cp->current_cmd);
                }

                cp->mode = CP_MODE_NORMAL;
                cp->current_cmd[0] = '\0';
                cp->cursor_pos = 0;
                cp->history_idx = -1;
                return 2;
            }
            return 0;
        } else if (key == KEY_UP) {
            if (cp->history_count > 0) {
                if (cp->history_idx == -1) {
                    cp->history_idx = cp->history_count - 1;
                } else if (cp->history_idx > 0) {
                    cp->history_idx--;
                }

                if (cp->history_idx >= 0 && cp->history_idx < cp->history_count) {
                    strcpy(cp->current_cmd, cp->cmd_history[cp->history_idx]);
                    cp->cursor_pos = strlen(cp->current_cmd);
                }
            }
            return 0;
        } else if (key == KEY_DOWN) {
            if (cp->history_idx != -1) {
                if (cp->history_idx < cp->history_count - 1) {
                    cp->history_idx++;
                    strcpy(cp->current_cmd, cp->cmd_history[cp->history_idx]);
                    cp->cursor_pos = strlen(cp->current_cmd);
                } else {
                    cp->history_idx = -1;
                    cp->current_cmd[0] = '\0';
                    cp->cursor_pos = 0;
                }
            }
            return 0;
        } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            if (cp->cursor_pos > 0) {
                cp->cursor_pos--;
                cp->current_cmd[cp->cursor_pos] = '\0';
            }
            cp->history_idx = -1;
            return 0;
        } else if (isprint(key) && cp->cursor_pos < CP_MAX_CMD_LEN - 1) {
            cp->current_cmd[cp->cursor_pos++] = key;
            cp->current_cmd[cp->cursor_pos] = '\0';
            cp->history_idx = -1;
            return 0;
        }
        return 0;
    } else {
        if (key == ':') {
            cp->mode = CP_MODE_CMD_INPUT;
            cp->current_cmd[0] = '\0';
            cp->cursor_pos = 0;
            return 0;
        } else if (key == 'v' || key == 'V') {
            if (cp->selected_file[0] && is_text_file(cp->selected_file)) {
                snprintf(output_cmd, cmd_size, "vim %s", cp->selected_file);
                return 1;
            }
            return 0;
        }
        return 0;
    }
}
