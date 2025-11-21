#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    pid_t child;
    int status;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <target>\n", argv[0]);
        return 1;
    }

    child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL); 
        // 자식이 자기 자신을 추적하도록 기본값으로 ptrace를 호출
        execl(argv[1], argv[1], NULL);
        perror("execl");
        return 1;
    } else {
        waitpid(child, &status, 0); // first stop (right away)
        printf("first child stop, start to trace event\n");

        while (1) {
            if (WIFEXITED(status)) {
                printf("child exited normally! status code: %d\n", WEXITSTATUS(status));
                break;
            }
            if (WIFSIGNALED(status)) {
                printf("child exited by signal! signo: %d\n", WTERMSIG(status));
                break;
            }
            if (WIFSTOPPED(status)) {
                printf("child stopped(SIGSTOP), signal: %d\n", WSTOPSIG(status));
            }
            ptrace(PTRACE_CONT, child, NULL, NULL);
            waitpid(child, &status, 0);
        }
    }
    return 0;
}