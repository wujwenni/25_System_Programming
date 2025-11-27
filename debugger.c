#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ncurses.h>

#define MAX_LINES 2000
#define MAX_LINE_LEN 256

typedef struct {
    char filename[256];
    char lines[MAX_LINES][MAX_LINE_LEN];
    int line_count;
} SourceCode;

SourceCode curCode = {0};

void load_source_file(const char* path);
void get_addr_line(const char*, unsigned long long, char*, size_t);
void draw_ui(int);

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
    char current_file[256];
    int current_line = 0;

    waitpid(child, &status, 0);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);

    mvprintw(10, 10, "Skipping library initialzation..");
    refresh();

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
            }
        }

        draw_ui(current_line);

        int ch = getch();

        if (ch == 'q' || ch == 'Q') {
            break;
        } else if (ch == KEY_F(10)) {
            
            char start_file[256];
            int start_line_num = current_line;
            strcpy(start_file, current_file);
            while(1) {
                ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                waitpid(child, &status, 0);
                
                if (WIFEXITED(status) || WIFSIGNALED(status)) break;

                ptrace(PTRACE_GETREGS, child, NULL, &regs);
                
                if (regs.rip >= 0x700000000000) continue;

                char temp_info[256];
                get_addr_line(argv[1], regs.rip, temp_info, sizeof(temp_info));
            
                char *temp_colon = strchr(temp_info, ':');
                if (temp_colon) {
                    *temp_colon = 0;
                    int new_line = atoi(temp_colon + 1);
                    if ((strcmp(temp_info, "??") != 0 && strcmp(temp_info, start_file) != 0) || 
                        (strcmp(temp_info, start_file) == 0 && new_line != start_line_num)) {
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
    curCode.line_count = 0;
    
    while (fgets(curCode.lines[curCode.line_count], MAX_LINE_LEN, fp) 
    && curCode.line_count < MAX_LINES) {
        curCode.lines[curCode.line_count][strcspn(curCode.lines[curCode.line_count], "\n")] = 0;
        curCode.line_count++;
    }
fclose(fp);
}

void get_addr_line(const char* exe, unsigned long long addr, char* buf, size_t size) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "addr2line -e %s %llx", exe, addr);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        fgets(buf, size, fp);
        buf[strcspn(buf, "\n")] = 0;
        pclose(fp);
    } else {
        strncpy(buf, "??:0", size);
    }
}

void draw_ui(int cur_line_num) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    attron(A_REVERSE);
    mvprintw(0, 0, "Devugger: %s | F10: Step | q: Quit", curCode.filename);
    mvprintw(0, width - 20, " Line: %d", cur_line_num);
    for (int i = 0; i < width; i++) mvaddch(1, i, ACS_HLINE);
    attroff(A_REVERSE);

    int view_height = height-3;
    int start_line = cur_line_num - (view_height/2);
    if (start_line < 0) start_line = 0;

    for (int i = 0; i < view_height; i++) {
        int file_line_idx = start_line + i;
        if (file_line_idx >= curCode.line_count) break;
        
        int screen_y = i + 2;
        
        if (file_line_idx + 1 == cur_line_num) {
            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(screen_y, 1, " %4d %s", file_line_idx + 1, curCode.lines[file_line_idx]);
            attroff(COLOR_PAIR(1) | A_BOLD);
        } 
        else {
            mvprintw(screen_y, 1, " %4d %s", file_line_idx + 1, curCode.lines[file_line_idx]);
            
        }
    } 
    refresh();
}