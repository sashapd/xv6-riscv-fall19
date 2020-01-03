#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int parent_fd[2];
  pipe(parent_fd);
  int child_fd[2];
  pipe(child_fd);
  char t;
  if (fork() == 0) {
    read(parent_fd[0], &t, 1);
    printf("%d: received ping\n", getpid());
    write(child_fd[1], "a", 1);
    close(parent_fd[0]);
    close(parent_fd[1]);
    exit(0);
  }
  write(parent_fd[1], "a", 1);
  read(child_fd[0], &t, 1);
  printf("%d: received pong\n", getpid());
  close(parent_fd[0]);
  close(parent_fd[1]);
  exit(0);
}
