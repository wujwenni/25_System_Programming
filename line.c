#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>

void addr2line(const char* exe, unsigned long long addr, char* buf, size_t size) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "addr2line -e %s %llx", exe, addr);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        fgets(buf, size, fp);
        buf[strcspn(buf, "\n")] = 0;
        pclose(fp);
    } else {
        strncpy(buf, "???", size);
    }
}

int main (int argc, char* argv[]) {
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
    } else {
        int status;
        struct user_regs_struct regs;
        char addrline[256] = {0}, prevline[256] = {0};
        waitpid(child, &status, 0);
        int steps = 0;

        setvbuf(stdout, NULL, _IONBF, 0); 
        printf("Debugger started. Skipping library initialization...\n");

        // 사용자 코드 영역에 도달할 때까지 자동 진행
        while (1) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                break; // 종료되면 루프 탈출
            }
            
            ptrace(PTRACE_GETREGS, child, NULL, &regs);
            
            // 사용자 코드 영역 (일반적으로 0x400000~0x600000 사이)
            if (regs.rip >= 0x400000 && regs.rip < 0x700000000000) {
                printf("Reached user code at RIP: 0x%llx\n", regs.rip);
                break; // 사용자 코드 진입, 디버깅 시작
            }
            
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            waitpid(child, &status, 0);
            steps++;
        }

        // 여기서부터 실제 줄 단위 디버깅 시작
        while (1) {
            if (WIFEXITED(status)) {
                printf("[EXIT] status: %d, steps: %d\n", WEXITSTATUS(status), steps);
                break;
            } else if (WIFSIGNALED(status)) {
                printf("[SIGEXIT] signo: %d, steps: %d\n", WTERMSIG(status), steps);
                break;
            }
            
            ptrace(PTRACE_GETREGS, child, NULL, &regs);
            
            // 라이브러리 영역은 스킵
            if (regs.rip >= 0x700000000000) {
                ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                waitpid(child, &status, 0);
                steps++;
                continue;
            }

            addr2line(argv[1], regs.rip, addrline, sizeof(addrline));

            if (strncmp(addrline, "??", 2) == 0) {
                ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                waitpid(child, &status, 0);
                steps++;
                continue;
            }

            if (strcmp(addrline, prevline) != 0) {
                printf("\n--- Source line: %s ---\n", addrline);
                printf("RIP: 0x%llx\n", regs.rip);
                strcpy(prevline, addrline);
                printf("Press Enter to step...");
                getchar();
            }
            
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            waitpid(child, &status, 0);
            steps++;
        }
    }
    return 0;
}
