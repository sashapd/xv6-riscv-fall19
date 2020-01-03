#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAX_ARG_LEN 32

int read_args(char args[MAXARG][MAX_ARG_LEN]) {
    char c;
    int i = 0;
    int j = 0;
    int success = 0;
    while(read(0, &c, 1) == 1 && c != '\n') {
        success = 1;
        if(c == ' ') {
            args[i][j] = '\0';
            i++;
            j = 0;
        } else {
            if (j >= MAX_ARG_LEN)
                continue;
            args[i][j] = c;
            j++;
        }
    }
    args[i][j] = '\0';
    return success ? i + 1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "usage: xargs program [arg1, arg2, ..]\n");
        exit(1);
    }
    if (strlen(argv[1]) > MAX_ARG_LEN) {
        fprintf(2, "xargs: path too long\n");
        exit(1);
    }

    char args[MAXARG + 1][MAX_ARG_LEN];
    int arg_size;

    while((arg_size = read_args(args)) > 0) {
        char *argvs[MAXARG];
        for(int i = 0; i < argc - 1; i++)
            argvs[i] = argv[i + 1];
        for(int i = argc - 1; i < MAXARG && i < arg_size + argc - 1; i++) {
            argvs[i] = args[i - argc + 1];
            argvs[i + 1] = 0;
        }
        if (fork() == 0) {
            if (exec(argv[1], argvs) != 0) {
                printf("exec failed");
                exit(1);
            }
        }
        int status;
        wait(&status);
    }
    exit(0);
}