#include "debugger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

void dbg_init(Debugger *dbg) {
    memset(dbg, 0, sizeof(Debugger));
    dbg->child_pid = -1;
    dbg->state = DBG_STATE_NOT_STARTED;
    dbg->instruction_count = 0;
    dbg->stdout_pipe[0] = -1;
    dbg->stdout_pipe[1] = -1;
    dbg->output_length = 0;
    dbg->current_line = 1;
    memset(dbg->output_buffer, 0, sizeof(dbg->output_buffer));
    dbg->addr2line_in = NULL;
    dbg->addr2line_out = NULL;
    dbg->addr2line_pid = -1;
    memset(dbg->error_message, 0, sizeof(dbg->error_message));
    dbg->error_signal = 0;
}

// Helper function to ensure clean state when restarting
static void cleanup_child_resources(Debugger *dbg) {
    // Drain and close stdout pipe
    if (dbg->stdout_pipe[0] != -1) {
        char discard_buf[4096];
        ssize_t n;
        while ((n = read(dbg->stdout_pipe[0], discard_buf, sizeof(discard_buf))) > 0) {
            // Discard all remaining data
        }
        close(dbg->stdout_pipe[0]);
        dbg->stdout_pipe[0] = -1;
    }

    if (dbg->stdout_pipe[1] != -1) {
        close(dbg->stdout_pipe[1]);
        dbg->stdout_pipe[1] = -1;
    }

    // Clear output buffer
    dbg->output_length = 0;
    memset(dbg->output_buffer, 0, sizeof(dbg->output_buffer));
}

// Start persistent addr2line process
static int start_addr2line(Debugger *dbg) {
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[0]);
        close(pipe_out[1]);

        execlp("addr2line", "addr2line", "-f", "-e", dbg->executable_path, NULL);
        perror("execlp addr2line");
        exit(1);
    } else {
        close(pipe_in[0]);
        close(pipe_out[1]);

        dbg->addr2line_in = fdopen(pipe_in[1], "w");
        dbg->addr2line_out = fdopen(pipe_out[0], "r");
        dbg->addr2line_pid = pid;

        if (!dbg->addr2line_in || !dbg->addr2line_out) {
            return -1;
        }

        setbuf(dbg->addr2line_in, NULL);
        setbuf(dbg->addr2line_out, NULL);

        return 0;
    }
}

// Stop persistent addr2line process
static void stop_addr2line(Debugger *dbg) {
    if (dbg->addr2line_in) {
        fclose(dbg->addr2line_in);
        dbg->addr2line_in = NULL;
    }
    if (dbg->addr2line_out) {
        fclose(dbg->addr2line_out);
        dbg->addr2line_out = NULL;
    }
    if (dbg->addr2line_pid > 0) {
        kill(dbg->addr2line_pid, SIGTERM);
        waitpid(dbg->addr2line_pid, NULL, 0);
        dbg->addr2line_pid = -1;
    }
}

// Set error message based on signal number
static void set_signal_error(Debugger *dbg, int signal_num) {
    dbg->error_signal = signal_num;
    switch (signal_num) {
        case SIGSEGV:
            snprintf(dbg->error_message, sizeof(dbg->error_message), "segfault");
            break;
        case SIGABRT:
            snprintf(dbg->error_message, sizeof(dbg->error_message), "aborted");
            break;
        case SIGFPE:
            snprintf(dbg->error_message, sizeof(dbg->error_message), "FPE");
            break;
        case SIGILL:
            snprintf(dbg->error_message, sizeof(dbg->error_message), "illegal insn");
            break;
        case SIGBUS:
            snprintf(dbg->error_message, sizeof(dbg->error_message), "bus error");
            break;
        default:
            snprintf(dbg->error_message, sizeof(dbg->error_message), "signal %d", signal_num);
            break;
    }
}


int dbg_load_program(Debugger *dbg, const char *executable_path, const char *source_path) {
    strncpy(dbg->executable_path, executable_path, 1023);
    strncpy(dbg->source_path, source_path, 1023);
    dbg->state = DBG_STATE_NOT_STARTED;

    dbg->output_length = 0;
    memset(dbg->output_buffer, 0, sizeof(dbg->output_buffer));

    if (start_addr2line(dbg) != 0) {
        return -1;
    }

    return 0;
}

int dbg_start(Debugger *dbg) {
    if (dbg->state != DBG_STATE_NOT_STARTED && dbg->state != DBG_STATE_EXITED) {
        return -1;
    }

    // Clean up any leftover resources from previous run
    cleanup_child_resources(dbg);

    memset(dbg->error_message, 0, sizeof(dbg->error_message));
    dbg->error_signal = 0;

    if (pipe(dbg->stdout_pipe) == -1) {
        dbg->state = DBG_STATE_ERROR;
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(dbg->stdout_pipe[0]);
        close(dbg->stdout_pipe[1]);
        dbg->state = DBG_STATE_ERROR;
        return -1;
    }

    if (pid == 0) {
        close(dbg->stdout_pipe[0]);
        dup2(dbg->stdout_pipe[1], STDOUT_FILENO);
        dup2(dbg->stdout_pipe[1], STDERR_FILENO);
        close(dbg->stdout_pipe[1]);

        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace TRACEME");
            exit(1);
        }

        char *args[] = {dbg->executable_path, NULL};
        execv(dbg->executable_path, args);
        perror("execv");
        exit(1);
    } else {
        close(dbg->stdout_pipe[1]);
        dbg->child_pid = pid;

        int flags = fcntl(dbg->stdout_pipe[0], F_GETFL, 0);
        fcntl(dbg->stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

        int status;
        waitpid(pid, &status, 0);

        if (!WIFSTOPPED(status)) {
            dbg->state = DBG_STATE_ERROR;
            return -1;
        }

        struct user_regs_struct regs;
        while (1) {
            if (WIFEXITED(status)) {
                dbg->state = DBG_STATE_EXITED;
                return -1;
            }

            if (WIFSIGNALED(status)) {
                dbg->state = DBG_STATE_ERROR;
                set_signal_error(dbg, WTERMSIG(status));
                return -1;
            }

            ptrace(PTRACE_GETREGS, pid, NULL, &regs);

            if (regs.rip >= 0x400000 && regs.rip < 0x700000000000) {
                break;
            }

            ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
            waitpid(pid, &status, 0);
        }

        dbg->state = DBG_STATE_STOPPED;
        update_regs(dbg);

        return 0;
    }
}

int dbg_stop(Debugger *dbg) {
    // Kill the child process first
    if (dbg->child_pid > 0) {
        ptrace(PTRACE_KILL, dbg->child_pid, NULL, NULL);
        waitpid(dbg->child_pid, NULL, 0);
        dbg->child_pid = -1;
    }

    // Clean up child resources (pipes and buffers)
    cleanup_child_resources(dbg);

    stop_addr2line(dbg);

    dbg->state = DBG_STATE_NOT_STARTED;
    return 0;
}
int dbg_step_line(Debugger *dbg) {
    if (dbg->state != DBG_STATE_STOPPED) {
        return -1;
    }

    int start_line = dbg->current_line;
    int status;
    int max_steps = 10000;

    for (int i = 0; i < max_steps; i++) {
        if (ptrace(PTRACE_SINGLESTEP, dbg->child_pid, NULL, NULL) == -1) {
            dbg->state = DBG_STATE_ERROR;
            return -1;
        }

        waitpid(dbg->child_pid, &status, 0);

        if (WIFEXITED(status)) {
            dbg->state = DBG_STATE_EXITED;
            return 0;
        }

        if (WIFSIGNALED(status)) {
            dbg->state = DBG_STATE_ERROR;
            set_signal_error(dbg, WTERMSIG(status));
            return 0;
        }

        if (!WIFSTOPPED(status)) {
            dbg->state = DBG_STATE_ERROR;
            snprintf(dbg->error_message, sizeof(dbg->error_message), "bad status");
            return -1;
        }

        int stop_signal = WSTOPSIG(status);
        if (stop_signal != SIGTRAP) {
            dbg->state = DBG_STATE_ERROR;
            set_signal_error(dbg, stop_signal);
            return 0;
        }

        update_regs(dbg);

        if (dbg->registers.rip >= 0x700000000000) {
            continue;
        }

        if (dbg->current_line != start_line && dbg->current_line > 0) {
            break;
        }
    }

    dbg->state = DBG_STATE_STOPPED;

    return 0;
}

int update_regs(Debugger *dbg) {
    if (dbg->child_pid <= 0) {
        return -1;
    }

    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, dbg->child_pid, NULL, &regs) == -1) {
        return -1;
    }

    dbg->registers.rax = regs.rax;
    dbg->registers.rbx = regs.rbx;
    dbg->registers.rcx = regs.rcx;
    dbg->registers.rdx = regs.rdx;
    dbg->registers.rsi = regs.rsi;
    dbg->registers.rdi = regs.rdi;
    dbg->registers.rbp = regs.rbp;
    dbg->registers.rsp = regs.rsp;
    dbg->registers.rip = regs.rip;
    dbg->registers.r8 = regs.r8;
    dbg->registers.r9 = regs.r9;
    dbg->registers.r10 = regs.r10;
    dbg->registers.r11 = regs.r11;
    dbg->registers.r12 = regs.r12;
    dbg->registers.r13 = regs.r13;
    dbg->registers.r14 = regs.r14;
    dbg->registers.r15 = regs.r15;

    dbg->current_rip = regs.rip;
    dbg->instruction_count++;

    dbg_get_current_line(dbg);

    return 0;
}

void dbg_get_current_line(Debugger *dbg) {
    if (dbg->child_pid <= 0 || !dbg->addr2line_in || !dbg->addr2line_out) {
        return;
    }

    fprintf(dbg->addr2line_in, "0x%llx\n", (unsigned long long)dbg->current_rip);

    char func_line[256];
    char result[1024];

    if (!fgets(func_line, sizeof(func_line), dbg->addr2line_out)) {
        return;
    }

    if (!fgets(result, sizeof(result), dbg->addr2line_out)) {
        return;
    }

    result[strcspn(result, "\n")] = 0;

    char *colon = strchr(result, ':');
    if (colon) {
        *colon = 0;
        if (strcmp(result, "??") != 0) {
            int new_line = atoi(colon + 1);
            if (new_line > 0) {
                dbg->current_line = new_line;
            }
        }
    }
}

void dbg_read_output(Debugger *dbg) {
    if (dbg->stdout_pipe[0] == -1) {
        return;
    }

    char temp_buf[1024];
    ssize_t n;

    while ((n = read(dbg->stdout_pipe[0], temp_buf, sizeof(temp_buf) - 1)) > 0) {
        int remaining = sizeof(dbg->output_buffer) - dbg->output_length - 1;
        if (n > remaining) {
            n = remaining;
        }
        if (n > 0) {
            memcpy(dbg->output_buffer + dbg->output_length, temp_buf, n);
            dbg->output_length += n;
            dbg->output_buffer[dbg->output_length] = '\0';
        }
        if (remaining <= 0) break;
    }
}

const char* dbg_state_string(DebuggerState state) {
    switch (state) {
        case DBG_STATE_NOT_STARTED: return "Not Started";
        case DBG_STATE_STOPPED: return "Stopped";
        case DBG_STATE_EXITED: return "Exited";
        case DBG_STATE_ERROR: return "Error";
        default: return "Unknown";
    }
}
