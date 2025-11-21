#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    pid_t child;
    int status;
    int steps = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <Target>\n", argv[0]);
        return 1;
    }

    child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(argv[1], argv[1], NULL);
        perror("execl");
        return 1;
    } else {
        waitpid(child, &status, 0);

        while (1) {
            if (WIFEXITED(status)) {
                printf("Child exited normally! status code: %d, total step: %d\n", 
                    WEXITSTATUS(status), steps);
                break;
            }
            else if (WIFSIGNALED(status)) {
                printf("Child exited by signal! signo: %d, total step: %d\n",
                    WTERMSIG(status), steps);
                break;
            }
            else if (WIFSTOPPED(status)) {
                struct user_regs_struct regs;
                ptrace(PTRACE_GETREGS, child, NULL, &regs);
                printf("[%d] Stopped at RIP: 0x%llx\n", steps, (unsigned long long)regs.rip);
            }
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            waitpid(child, &status, 0);
            steps++;
        }
    }
    return 0;
}