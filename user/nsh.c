#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX_CMD 100

int main(int argc, char *argv[])
{
    static char buf[MAX_CMD];
    int fd;

    while((fd = open("console", O_RDWR)) >= 0){
        if(fd >= 3) {
            close(fd);
            break;
        }
    }

    

    exit(0);
}
