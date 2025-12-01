#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ncurses.h>

#define MAX_LINES     2000
#define MAX_LINE_LEN  256
#define MAX_VLINES    (MAX_LINES * 8)

typedef struct {
    char filename[256];
    char lines[MAX_LINES][MAX_LINE_LEN];
    int  line_count;
} SourceCode;

typedef struct {
    int src_line;   // 0-based 논리 라인
    int wrap_idx;   // 그 라인 안에서 몇 번째 접힌 줄인지
} VisualLine;

SourceCode curCode = {0};
VisualLine vlines[MAX_VLINES];
int vline_count = 0;

int gutter_width = 8;   // 화살표 + 라인번호 영역
int view_start  = 0;    // 현재 페이지의 첫 visual line index

void load_source_file(const char* path);
void get_addr_line(const char*, unsigned long long, char*, size_t);
void rebuild_visual_lines(int term_width);
int  visual_index_for_line(int logical_line); // 1-based 논리 라인
void draw_ui(int cur_logical_line);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <target>\n", argv[0]);
        return 1;
    }

    pid_t child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(argv[1], argv[1], NULL);
        perror("execl");
        return 1;
    }

    int status;
    struct user_regs_struct regs;
    char addr_info[256];
    char current_file[256] = "";
    int  current_line = 0;

    waitpid(child, &status, 0);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);

    int h, w;
    getmaxyx(stdscr, h, w);
    rebuild_visual_lines(w);

    mvprintw(10, 10, "Skipping library initialization..");
    refresh();

    // 라이브러리 초기화 스킵 루프 (원래 코드 그대로)
    while (1) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) break;

        ptrace(PTRACE_GETREGS, child, NULL, &regs);

        if (regs.rip >= 0x400000 && regs.rip < 0x700000000000) {
            break;
        }
        ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
        waitpid(child, &status, 0);
    }

    while (1) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) break;

        ptrace(PTRACE_GETREGS, child, NULL, &regs);
        get_addr_line(argv[1], regs.rip, addr_info, sizeof(addr_info));

        char* colon = strchr(addr_info, ':');
        if (colon) {
            *colon = 0;
            strcpy(current_file, addr_info);
            current_line = atoi(colon + 1);

            if (strcmp(current_file, "??") != 0) {
                load_source_file(current_file);
                getmaxyx(stdscr, h, w);
                rebuild_visual_lines(w);
            }
        }

        // 논리 라인에 맞춰 view_start 조정 (페이지 단위)
        int v_idx = visual_index_for_line(current_line); // 0-based
        if (v_idx >= 0) {
            int view_height = h - 3;
            int center = v_idx - view_height / 2;
            if (center < 0) center = 0;
            if (center > vline_count - view_height)
                center = (vline_count > view_height) ? vline_count - view_height : 0;
            view_start = center;
        }

        draw_ui(current_line);

        int ch = getch();

        if (ch == KEY_RESIZE) {
            getmaxyx(stdscr, h, w);
            rebuild_visual_lines(w);
            draw_ui(current_line);
            continue;
        }

        if (ch == 'q' || ch == 'Q') {
            break;
        } else if (ch == KEY_F(10)) {
            char start_file[256];
            int start_line_num = current_line;
            strcpy(start_file, current_file);

            while (1) {
                ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                waitpid(child, &status, 0);

                if (WIFEXITED(status) || WIFSIGNALED(status)) break;

                ptrace(PTRACE_GETREGS, child, NULL, &regs);
                if (regs.rip >= 0x700000000000) continue;

                char temp_info[256];
                get_addr_line(argv[1], regs.rip, temp_info, sizeof(temp_info));

                char* temp_colon = strchr(temp_info, ':');
                if (temp_colon) {
                    *temp_colon = 0;
                    int new_line = atoi(temp_colon + 1);
                    if ((strcmp(temp_info, "??") != 0 &&
                         strcmp(temp_info, start_file) != 0) ||
                        (strcmp(temp_info, start_file) == 0 &&
                         new_line != start_line_num)) {
                        break;
                    }
                }
            }
        }
    }

    endwin();
    printf("Target exited\n");
    return 0;
}

void load_source_file(const char* path) {
    if (strcmp(curCode.filename, path) == 0) return;

    FILE* fp = fopen(path, "r");
    if (!fp) {
        curCode.line_count = 0;
        snprintf(curCode.filename, sizeof(curCode.filename),
                 "%s File is not found)\n", path);
        return;
    }

    strncpy(curCode.filename, path, sizeof(curCode.filename) - 1);
    curCode.filename[sizeof(curCode.filename) - 1] = '\0';
    curCode.line_count = 0;

    while (curCode.line_count < MAX_LINES &&
           fgets(curCode.lines[curCode.line_count], MAX_LINE_LEN, fp)) {
        curCode.lines[curCode.line_count]
            [strcspn(curCode.lines[curCode.line_count], "\n")] = 0;
        curCode.line_count++;
    }
    fclose(fp);
}

void get_addr_line(const char* exe, unsigned long long addr,
                   char* buf, size_t size) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "addr2line -e %s %llx", exe, addr);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buf, size, fp)) {
            buf[strcspn(buf, "\n")] = 0;
        } else {
            strncpy(buf, "??:0", size);
            buf[size-1] = '\0';
        }
        pclose(fp);
    } else {
        strncpy(buf, "??:0", size);
        buf[size-1] = '\0';
    }
}

// term_width를 기준으로 VisualLine 배열 재구성
void rebuild_visual_lines(int term_width) {
    vline_count = 0;

    int code_start_x = gutter_width;
    int code_width = term_width - code_start_x - 1;
    if (code_width <= 0) code_width = 1;

    for (int i = 0; i < curCode.line_count; i++) {
        int len = (int)strlen(curCode.lines[i]);
        int needed = (len == 0) ? 1 : ( (len + code_width - 1) / code_width );

        for (int w = 0; w < needed && vline_count < MAX_VLINES; w++) {
            vlines[vline_count].src_line = i;
            vlines[vline_count].wrap_idx = w;
            vline_count++;
        }
    }
}

// 1-based logical_line → 해당하는 첫 visual index (0-based), 없으면 -1
int visual_index_for_line(int logical_line) {
    int target = logical_line - 1;
    if (target < 0) return -1;

    for (int i = 0; i < vline_count; i++) {
        if (vlines[i].src_line == target) {
            return i;
        }
    }
    return -1;
}

void draw_ui(int cur_logical_line) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    attron(A_REVERSE);
    mvprintw(0, 0, "Debugger: %s | F10: Step | q: Quit", curCode.filename);
    mvprintw(0, width - 20, " Line: %d", cur_logical_line);
    for (int i = 0; i < width; i++) mvaddch(1, i, ACS_HLINE);
    attroff(A_REVERSE);

    int view_height = height - 3;
    if (view_height < 1) {
        refresh();
        return;
    }

    for (int i = 0; i < view_height; i++) {
        int v_idx = view_start + i;
        if (v_idx < 0 || v_idx >= vline_count) break;

        int screen_y = i + 2;

        int visual_no = v_idx + 1;
        mvprintw(screen_y, 0, "%4d ", visual_no);

        int src = vlines[v_idx].src_line;
        int wrap = vlines[v_idx].wrap_idx;

        int logical_line_num = src + 1;

        // 거터 + 라인 번호
        if (logical_line_num == cur_logical_line) {
            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(screen_y, 0, "->");
            attroff(COLOR_PAIR(1) | A_BOLD);
        }

        // 코드 본문에서 해당 wrap 부분만 출력
        int code_start_x = gutter_width;
        int code_width   = width - code_start_x - 1;
        if (code_width <= 0) code_width = 1;

        const char* line = curCode.lines[src];
        int len = (int)strlen(line);
        int offset = wrap * code_width;
        if (offset < len) {
            char buf[1024];
            int copy_len = len - offset;
            if (copy_len > code_width) copy_len = code_width;
            if (copy_len > (int)sizeof(buf) - 1) copy_len = sizeof(buf) - 1;

            memcpy(buf, line + offset, copy_len);
            buf[copy_len] = '\0';

            mvprintw(screen_y, code_start_x, "%s", buf);
        }
    }

    refresh();
}
