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
    dbg->gdb_in = NULL;
    dbg->gdb_out = NULL;
    dbg->gdb_pid = -1;
    dbg->variable_count = 0;
}

// Start persistent addr2line process
static int start_addr2line(Debugger *dbg) {
    int pipe_in[2];   // Parent writes, addr2line reads
    int pipe_out[2];  // addr2line writes, parent reads

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
        // Child: addr2line process
        close(pipe_in[1]);   // Close write end of input
        close(pipe_out[0]);  // Close read end of output

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[0]);
        close(pipe_out[1]);

        // Run addr2line in interactive mode (-f for function names, -e for executable)
        execlp("addr2line", "addr2line", "-f", "-e", dbg->executable_path, NULL);
        perror("execlp addr2line");
        exit(1);
    } else {
        // Parent
        close(pipe_in[0]);   // Close read end of input
        close(pipe_out[1]);  // Close write end of output

        dbg->addr2line_in = fdopen(pipe_in[1], "w");
        dbg->addr2line_out = fdopen(pipe_out[0], "r");
        dbg->addr2line_pid = pid;

        if (!dbg->addr2line_in || !dbg->addr2line_out) {
            return -1;
        }

        // Disable buffering for immediate response
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

// Start GDB MI process for variable tracking

// Stop GDB MI process
static void stop_gdb_mi(Debugger *dbg) {
    if (dbg->gdb_in) {
        fprintf(dbg->gdb_in, "-gdb-exit\n");
        fclose(dbg->gdb_in);
        dbg->gdb_in = NULL;
    }
    if (dbg->gdb_out) {
        fclose(dbg->gdb_out);
        dbg->gdb_out = NULL;
    }
    if (dbg->gdb_pid > 0) {
        kill(dbg->gdb_pid, SIGTERM);
        waitpid(dbg->gdb_pid, NULL, 0);
        dbg->gdb_pid = -1;
    }
}

int dbg_load_program(Debugger *dbg, const char *executable_path, const char *source_path) {
    strncpy(dbg->executable_path, executable_path, sizeof(dbg->executable_path) - 1);
    strncpy(dbg->source_path, source_path, sizeof(dbg->source_path) - 1);
    dbg->state = DBG_STATE_NOT_STARTED;

    // Clear output buffer for new session
    dbg->output_length = 0;
    memset(dbg->output_buffer, 0, sizeof(dbg->output_buffer));

    // Start persistent addr2line process
    if (start_addr2line(dbg) != 0) {
        return -1;
    }

    return 0;
}

int dbg_start(Debugger *dbg) {
    if (dbg->state != DBG_STATE_NOT_STARTED && dbg->state != DBG_STATE_EXITED) {
        return -1;
    }

    // Create pipe for capturing stdout/stderr
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
        // Child process
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
        // Parent process
        close(dbg->stdout_pipe[1]);
        dbg->child_pid = pid;

        // Set pipe to non-blocking
        int flags = fcntl(dbg->stdout_pipe[0], F_GETFL, 0);
        fcntl(dbg->stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

        int status;
        waitpid(pid, &status, 0);

        if (!WIFSTOPPED(status)) {
            dbg->state = DBG_STATE_ERROR;
            return -1;
        }

        // Skip library initialization (like old_dbg.c lines 77-87)
        struct user_regs_struct regs;
        while (1) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                dbg->state = DBG_STATE_EXITED;
                return -1;
            }

            ptrace(PTRACE_GETREGS, pid, NULL, &regs);

            // When RIP is in user code range, stop
            if (regs.rip >= 0x400000 && regs.rip < 0x700000000000) {
                break;
            }

            ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
            waitpid(pid, &status, 0);
        }

        dbg->state = DBG_STATE_STOPPED;
        dbg_update_registers(dbg);

        return 0;
    }
}

int dbg_stop(Debugger *dbg) {
    if (dbg->child_pid > 0) {
        ptrace(PTRACE_KILL, dbg->child_pid, NULL, NULL);
        waitpid(dbg->child_pid, NULL, 0);
        dbg->child_pid = -1;
    }
    if (dbg->stdout_pipe[0] != -1) {
        close(dbg->stdout_pipe[0]);
        dbg->stdout_pipe[0] = -1;
    }

    // Stop addr2line and GDB processes
    stop_addr2line(dbg);
    stop_gdb_mi(dbg);

    // Clear output buffer when stopping
    dbg->output_length = 0;
    memset(dbg->output_buffer, 0, sizeof(dbg->output_buffer));

    dbg->state = DBG_STATE_NOT_STARTED;
    return 0;
}

// Step until source line changes (like old_dbg.c F10 handler)
int dbg_step_line(Debugger *dbg) {
    if (dbg->state != DBG_STATE_STOPPED) {
        return -1;
    }

    int start_line = dbg->current_line;
    int status;
    int max_steps = 10000;

    for (int i = 0; i < max_steps; i++) {
        // Single step
        if (ptrace(PTRACE_SINGLESTEP, dbg->child_pid, NULL, NULL) == -1) {
            dbg->state = DBG_STATE_ERROR;
            return -1;
        }

        waitpid(dbg->child_pid, &status, 0);

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            dbg->state = DBG_STATE_EXITED;
            return 0;
        }

        if (!WIFSTOPPED(status)) {
            dbg->state = DBG_STATE_ERROR;
            return -1;
        }

        dbg_update_registers(dbg);

        // Skip if in library code
        if (dbg->registers.rip >= 0x700000000000) {
            continue;
        }

        // Check if line changed
        if (dbg->current_line != start_line && dbg->current_line > 0) {
            break;
        }
    }

    dbg->state = DBG_STATE_STOPPED;

    // Update variables after stepping
    dbg_update_variables(dbg);

    return 0;
}

int dbg_update_registers(Debugger *dbg) {
    if (dbg->child_pid <= 0) {
        return -1;
    }

    // Save previous values
    dbg->prev_registers.rax = dbg->registers.rax;
    dbg->prev_registers.rbx = dbg->registers.rbx;
    dbg->prev_registers.rcx = dbg->registers.rcx;
    dbg->prev_registers.rdx = dbg->registers.rdx;
    dbg->prev_registers.rip = dbg->registers.rip;
    dbg->prev_registers.rsp = dbg->registers.rsp;
    dbg->prev_registers.rbp = dbg->registers.rbp;

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

    // Update current line using addr2line
    dbg_get_current_line(dbg);

    return 0;
}

void dbg_get_current_line(Debugger *dbg) {
    if (dbg->child_pid <= 0 || !dbg->addr2line_in || !dbg->addr2line_out) {
        return;
    }

    // Send address to addr2line (format: "0x12345678\n")
    fprintf(dbg->addr2line_in, "0x%llx\n", (unsigned long long)dbg->current_rip);

    // Read two lines: function name (we ignore), then filename:line
    char func_line[256];
    char result[1024];

    if (!fgets(func_line, sizeof(func_line), dbg->addr2line_out)) {
        return;
    }

    if (!fgets(result, sizeof(result), dbg->addr2line_out)) {
        return;
    }

    result[strcspn(result, "\n")] = 0;

    // Parse "filename:line" format
    char *colon = strchr(result, ':');
    if (colon) {
        *colon = 0;
        // Only update if it's not "??"
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

long dbg_read_memory(Debugger *dbg, unsigned long address) {
    if (dbg->child_pid <= 0) {
        return 0;
    }

    errno = 0;
    long value = ptrace(PTRACE_PEEKDATA, dbg->child_pid, address, NULL);
    if (errno != 0) {
        return 0;
    }
    return value;
}

// Use readelf to parse DWARF and find variable locations
void dbg_update_variables(Debugger *dbg) {
    if (dbg->child_pid <= 0 || dbg->state != DBG_STATE_STOPPED) {
        dbg->variable_count = 0;
        return;
    }

    dbg->variable_count = 0;

    // Use readelf to get DWARF debug info for variables
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "readelf --debug-dump=info %s 2>/dev/null | "
             "grep -B 2 -A 5 'DW_TAG_variable'",
             dbg->executable_path);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        // Fallback: simple stack reading
        for (int i = 0; i < 3; i++) {
            snprintf(dbg->variables[i].name, 64, "var_%d", i);
            dbg->variables[i].value = dbg_read_memory(dbg, dbg->registers.rbp - ((i+1)*8));
            dbg->variables[i].valid = 1;
            dbg->variables[i].is_pointer = 0;
        }
        dbg->variable_count = 3;
        return;
    }

    char line[512];
    char current_var[64] = {0};

    while (fgets(line, sizeof(line), fp) && dbg->variable_count < DBG_MAX_VARS) {
        // Look for variable name: DW_AT_name : x
        if (strstr(line, "DW_AT_name") && strstr(line, ": ")) {
            char *colon = strstr(line, ": ");
            if (colon) {
                colon += 2;
                while (*colon == ' ') colon++;

                int i = 0;
                while (colon[i] && !isspace(colon[i]) && i < 63) {
                    current_var[i] = colon[i];
                    i++;
                }
                current_var[i] = '\0';
            }
        }

        // Look for location: DW_AT_location : ... fbreg: -X
        if (current_var[0] && strstr(line, "DW_AT_location") && strstr(line, "fbreg")) {
            char *fbreg = strstr(line, "fbreg: ");
            if (fbreg) {
                int offset = atoi(fbreg + 7);

                strncpy(dbg->variables[dbg->variable_count].name, current_var, 63);
                dbg->variables[dbg->variable_count].value =
                    dbg_read_memory(dbg, dbg->registers.rbp + offset);
                dbg->variables[dbg->variable_count].valid = 1;
                dbg->variables[dbg->variable_count].is_pointer = 0;
                dbg->variable_count++;

                current_var[0] = '\0';
            }
        }
    }
    pclose(fp);

    // If no variables found via DWARF, use simple stack reading
    if (dbg->variable_count == 0) {
        for (int i = 0; i < 3; i++) {
            snprintf(dbg->variables[i].name, 64, "[rbp-%d]", (i+1)*8);
            dbg->variables[i].value = dbg_read_memory(dbg, dbg->registers.rbp - ((i+1)*8));
            dbg->variables[i].valid = 1;
            dbg->variables[i].is_pointer = 0;
        }
        dbg->variable_count = 3;
    }
}

const char* dbg_state_string(DebuggerState state) {
    switch (state) {
        case DBG_STATE_NOT_STARTED: return "Not Started";
        case DBG_STATE_RUNNING: return "Running";
        case DBG_STATE_STOPPED: return "Stopped";
        case DBG_STATE_EXITED: return "Exited";
        case DBG_STATE_ERROR: return "Error";
        default: return "Unknown";
    }
}
