#include "kernel/types.h"
#include "user/user.h"

#define BYTE_MASK 0xFF

int create_neighbor();
void destroy_neighbor(int fd);
void process(int left_fd);
int read_int(int fd);
void write_int(int fd, int n);

int main()
{
    int fd = create_neighbor();
    for (int n = 2; n < 38; n++) {
        write_int(fd, n);
    }
    destroy_neighbor(fd);

    return 0;
}

int create_neighbor()
{
    int p[2];
    pipe(p);

    int pid = fork();
    if (pid > 0) {
        close(p[0]);
        return p[1];
    } else {
        close(p[1]);
        process(p[0]);
        exit(0);
    }
}

void destroy_neighbor(int fd)
{
    if (fd > 0) {
        close(fd);
        wait(0);
    }
}

void process(int left_fd)
{
    int right_fd = 0;

    int p = read_int(left_fd);
    if (p == 0) {
        destroy_neighbor(right_fd);
        return;
    }

    printf("prime %d\n", p);

    int n;
    while (1) {
        n = read_int(left_fd);
        if (n == 0) {
            destroy_neighbor(right_fd);
            return;
        }

        if (n % p != 0) {
            if (right_fd == 0) {
                right_fd = create_neighbor();
            }

            write_int(right_fd, n);
        }
    }
}

int read_int(int fd)
{
    char buf[4];
    int r = read(fd, buf, 4);
    if (r == 0) {
        return 0;
    }

    if (r < 4) {
        printf("Failed to read int: bytes read = %d", r);
        return 0;
    }

    int n = 0;
    n |= buf[0];
    n |= buf[1] <<  8;
    n |= buf[2] << 16;
    n |= buf[3] << 24;

    return n;
}

void write_int(int fd, int n)
{
    char buf[4];
    buf[0] = n & BYTE_MASK;
    buf[1] = (n >>  8) & BYTE_MASK;
    buf[2] = (n >> 16) & BYTE_MASK;
    buf[3] = (n >> 24) & BYTE_MASK;

    write(fd, buf, 4);
}
