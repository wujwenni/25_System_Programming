#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
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
        ptrace(PTRACE_TRACEME, 0 , NULL, NULL);
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
             if (WIFSIGNALED(status)) {
                printf("child exited by signal! signo: %d, total step: %d\n", 
                    WTERMSIG(status), steps);
                break;
            }
            if (WIFSTOPPED(status)) {
                printf("child stopped with SIGSTOP, signal: %d, total step: %d\n", 
                    WSTOPSIG(status), steps);
            }
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            waitpid(child, &status, 0);
            steps++;
        }
    }
    return 0;
}