#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    DBG_STATE_NOT_STARTED,
    DBG_STATE_STOPPED,
    DBG_STATE_EXITED,
    DBG_STATE_ERROR
} DebuggerState;

typedef struct {
    pid_t child_pid;
    DebuggerState state;

    char executable_path[1024];
    char source_path[1024];

    // Current execution state
    unsigned long current_rip;
    int current_line;
    int instruction_count;

    // Registers (current)
    struct {
        unsigned long rax, rbx, rcx, rdx;
        unsigned long rsi, rdi, rbp, rsp, rip;
        unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
    } registers;

    // Program output
    int stdout_pipe[2];
    char output_buffer[4096];
    int output_length;

    // Persistent addr2line process for fast address lookup
    FILE *addr2line_in;   // Write addresses here
    FILE *addr2line_out;  // Read results from here
    pid_t addr2line_pid;

    // Error information
    char error_message[256];
    int error_signal;

} Debugger;

// Core functions
void dbg_init(Debugger *dbg);
int dbg_load_program(Debugger *dbg, const char *executable_path, const char *source_path);
int dbg_start(Debugger *dbg);
int dbg_stop(Debugger *dbg);

// Step execution - steps until source line changes
int dbg_step_line(Debugger *dbg);

// Information retrieval
int update_regs(Debugger *dbg);
void dbg_get_current_line(Debugger *dbg);  // Use addr2line
void dbg_read_output(Debugger *dbg);
const char* dbg_state_string(DebuggerState state);

#endif
