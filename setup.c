#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv []) {
    pid_t child;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <File name>\n", argv[0]);
        return 1;
    }

    child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(argv[1], argv[1], NULL);
        perror("execl");
        return 1;
    } else {
        wait(NULL);
        printf("child start, it's ready to trace!\n");
    }
    return 0;
}