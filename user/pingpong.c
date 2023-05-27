#include "kernel/types.h"
#include "user/user.h"

int main() {
    int p1[2];
    pipe(p1);

    int p2[2];
    pipe(p2);

    char byte[1];
    int pid = fork();
    if (pid > 0) {
        close(p1[0]);
        close(p2[1]);
        byte[0] = (char) 0xFF;
        write(p1[1], byte, 1);
        read(p2[0], byte, 1);
        printf("%d: received pong\n", getpid());
    } else {
        close(p1[1]);
        close(p2[0]);
        read(p1[0], byte, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], byte, 1);
    }

    return 0;
}
