#include "kernel/types.h"
#include "user/user.h"

// get numbers from left neighbor
// send numbers to right neighbor
// wait until child process finishes

// 0 --> read, 1 --> write

void recursion_processing(int pfd[2]) {
  int pid, cfd[2], p, n, end = -1, status;
  close(pfd[1]);
  read(pfd[0], &p, 4);
  if (p == -1) {
    exit(0);
  }
  printf("prime %d\n", p);
  pipe(cfd);
  // only create child when needed
  if ((pid = fork()) == 0) {
    // child process
    recursion_processing(cfd);
  } else {
    // parent process
    close(cfd[0]);
    while (1) {
      read(pfd[0], &n, 4);
      if (n == -1) {
        break;
      }
      if (n%p != 0) {
        write(cfd[1], &n, 4);
      }
    }
    write(cfd[1], &end, 4);
    // wait for child process to finish
    wait(&status);
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  int pid, cfd[2], end = -1;
  pipe(cfd);
  if ((pid = fork()) == 0) {
    // first child process
    recursion_processing(cfd);
  } else {
    // top process
    int p = 2, status;
    printf("prime %d\n", p);
    close(cfd[0]);
    for (int n = p+1; n < 35; ++n) {
      if (n%p != 0) {
        write(cfd[1], &n, 4);
      }
    }
    // write -1 when finished
    write(cfd[1], &end, 4);
    // wait for child process to finish
    wait(&status);
  }
  exit(0);
}