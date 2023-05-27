#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXSTR 512

int getline(char *buf, int max);

int main(int argc, char *argv[])
{
    int cmdargc = 0;
    char *cmdargv[MAXARG + 1];
    for (int i = 1; i < argc; i++) {
        cmdargv[cmdargc++] = argv[i];
    }

    int n, pid;
    char buf[MAXSTR];
    while ((n = getline(buf, MAXSTR)) > 0) {
        if (buf[n - 1] == '\n') {
            buf[n - 1] = '\0';
        }

        cmdargv[cmdargc] = buf;
        cmdargv[cmdargc + 1] = 0;

        pid = fork();
        if (pid > 0) {
            wait(0);
        } else if (pid == 0) {
            exec(cmdargv[0], cmdargv);
            fprintf(2, "oops");
            return 1;
        } else {
            fprintf(2, "can't fork");
            return 1;
        }
    }

    return 0;
}

int getline(char *buf, int max)
{
    char ch;
    int i = 0;
    while (i < max - 1) {
        if (read(0, &ch, 1) < 1) {
            break;
        }

        buf[i++] = ch;
        if (ch == '\n') {
            break;
        }
    }
    buf[i] = '\0';

    return i;
}
