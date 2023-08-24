#include "kernel/types.h"
#include "user/user.h"

// int pipe(int p[]) 
// p[0] --> read | p[1] --> write

int main(int argc, char *argv[]) {
  int pid, pfd[2], cfd[2];
  pipe(pfd);
  pipe(cfd);
  char buf[20];
  if ((pid = fork()) == 0) {
    // child process
    // receive ping
    close(cfd[1]);
    read(cfd[0], buf, sizeof(buf));
    printf("%d: received %s\n", getpid(), buf);
    // write pong
    close(pfd[0]);
    write(pfd[1], "pong", 4);
    exit(0);
  } else {
    // parent process
    // write ping
    close(cfd[0]);
    write(cfd[1], "ping", 4);
    // receive pong
    close(pfd[1]);
    read(pfd[0], buf, sizeof(buf));
    printf("%d: received %s\n", getpid(), buf);
    exit(0);
  }
}