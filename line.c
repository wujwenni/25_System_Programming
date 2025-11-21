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

        // 진행 상황을 보기 위해 버퍼링 끔
        setvbuf(stdout, NULL, _IONBF, 0); 
        printf("Debugger started. Skipping library initialization...\n");

        while (1) {
            if (WIFEXITED(status)) {
                printf("[EXIT] status: %d, steps: %d\n", WEXITSTATUS(status), steps);
                break;
            } else if (WIFSIGNALED(status)) {
                printf("[SIGEXIT] signo: %d, steps: %d\n", WTERMSIG(status), steps);
                break;
            } else if (WIFSTOPPED(status)) {
                ptrace(PTRACE_GETREGS, child, NULL, &regs);

                // [핵심 수정] 최적화: 라이브러리 영역(높은 주소)은 addr2line 수행 없이 무조건 스킵
                // 64비트 리눅스에서 사용자 코드는 보통 0x400000 근처, 라이브러리는 0x7f... 에 위치함
                // 따라서 RIP가 0x700000000000 보다 크면 라이브러리라고 판단하고 스킵.
                if (regs.rip >= 0x700000000000) {
                    ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                    waitpid(child, &status, 0);
                    steps++;
                    continue; 
                }

                // 여기부터는 사용자 영역 코드이므로 addr2line 수행
                addr2line(argv[1], regs.rip, addrline, sizeof(addrline));

                // 혹시 모를 '??' 처리
                if (strncmp(addrline, "??", 2) == 0) {
                    ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                    waitpid(child, &status, 0);
                    steps++;
                    continue;
                }

                if (strcmp(addrline, prevline) != 0) {
                    printf("\n--- Source line: %s ---\n", addrline);
                    printf("RIP: 0x%llx\n", regs.rip);
                    
                    // 타겟 프로그램의 변수나 상태는 메모리를 직접 읽어야 보임 (지금은 생략)
                    
                    strcpy(prevline, addrline);
                    printf("Press Enter to step...");
                    getchar();
                    // sleep(3); // getchar가 있으므로 sleep은 불필요
                }
            }
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            waitpid(child, &status, 0);
            steps++;
        }
    }
    return 0;
}