#include "kernel/types.h"
#include "user/user.h"

void read_left(int read_fd)
{
  int prime;
  if (read(read_fd, &prime, 4) == 4)
  {
    printf("prime %d\n", prime);
    int write_fd = -1;
    int num;
    int ret;
    while ((ret = read(read_fd, &num, 4)) == 4)
    {
      if (ret == -1) {
        printf("read failed\n");
      }
      if (num % prime != 0)
      {
        if (write_fd == -1)
        {
          int fds[2];
          if (pipe(fds) == -1) {
            printf("pipe faile\n");
          }
          write_fd = fds[1];
          ret = fork();
          if (ret == 0)
          {
            if (close(fds[1])) {
              printf("close failed\n");
            }
            if (close(read_fd)) {
              printf("close failed\n");
            }
            read_left(fds[0]);
          } else if(ret == -1) {
            printf("fork failed\n");
          }
          if (close(fds[0])) {
            printf("close failed\n");
          }
        }
        ret = write(write_fd, &num, 4);
        if (ret == -1) {
          printf("write failed\n");
        }
      }
    }
    if (write_fd != -1) {
      ret = close(write_fd);
      if (ret == -1) {
        printf("close failed\n");
      }
    }
  }
  close(read_fd);
  int status;
  wait(&status);
  exit(0);
}

int main(int argc, char *argv[])
{
  int fds[2];
  pipe(fds);
  if (fork() == 0)
  {
    close(fds[1]);
    read_left(fds[0]);
  }
  close(fds[0]);
  for (int i = 2; i < 35; ++i)
  {
    write(fds[1], &i, 4);
  }
  close(fds[1]);
  int status;
  wait(&status);
  exit(0);
}