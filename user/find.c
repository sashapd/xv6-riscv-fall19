#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/stat.h"

#define PATH_MAX 512

void find(char *path, const char *filename)
{
    char *p;
    int fd;
    struct stat st;
    struct dirent de;
    int path_len = strlen(path);
    int file_len = strlen(filename);

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        if (file_len < path_len && strcmp(filename, path + path_len - file_len) == 0)
            printf("%s\n", path);
        break;
    case T_DIR:
        if (path_len + 1 + DIRSIZ + 1 > PATH_MAX) {
            printf("find: path too long\n");
            break;
        }
        p = path + path_len;
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if(stat(path, &st) < 0){
                printf("find: cannot stat %s\n", path);
                continue;
            }
            find(path, filename);
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(2, "usage: find path filename\n");
        exit(1);
    }
    int path_len = strlen(argv[1]);
    if (path_len >= PATH_MAX / 2)
    {
        fprintf(2, "File path too long\n");
        exit(1);
    }
    char path[PATH_MAX];
    strcpy(path, argv[1]);
    find(path, argv[2]);
    exit(0);
}