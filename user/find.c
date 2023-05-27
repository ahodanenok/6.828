#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

void find(char *dirname, char *searchstr);

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Not enough arguments\n");
        printf("Usage: find <directory> <file-name>\n");
        return 1;
    }

    find(argv[1], argv[2]);

    return 0;
}

void find(char *dirname, char *searchstr)
{
    struct dirent de;
    struct stat st;
    char path[512];
    int dirlen = strlen(dirname);

    if (dirlen + 1 + DIRSIZ + 1 > sizeof(path)) {
        printf("path is to large: '%s'\n", dirname);
        return;
    }

    strcpy(path, dirname);
    path[dirlen] = '/';

    int fd = open(dirname, 0);
    if (fd < 0) {
        fprintf(2, "can't open directory '%s'\n", dirname);
        return;
    }

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            continue;
        }

        strcpy(path + dirlen + 1, de.name);
        path[dirlen + 1 + strlen(de.name)] = '\0';

        if (stat(path, &st) < 0) {
            fprintf(2, "can't stat '%s'\n", path);
            return;
        }

        if (st.type == T_FILE && strcmp(de.name, searchstr) == 0) {
            printf("%s\n", path);
        } else if (st.type == T_DIR) {
            find(path, searchstr);
        }
    }

    close(fd);
}
