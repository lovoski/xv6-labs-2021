#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

// use `|` to concatenate two commands
// the stdout of the first command will be
// connected to the stdin of the second program
// in xv6, read from stdin using gets function

// echo hello too | xargs echo bye

void readline(char *buf, int maxlen) {
  int i, cc;
  char c;
  for(i=0; i+1 < maxlen; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i-1] = '\0';
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("not enough params\n");
    exit(-1);
  }
  char buf[512], *nargv[MAXARG];
  for (int i = 1; i < argc; ++i) {
    nargv[i-1] = argv[i];
  }
  int pid, status;
  while (1) {
    // read from stdin
    // concatenate the input to argv commmand
    readline(buf, sizeof(buf));
    if (buf[0] == '\0') {
      break;
    }
    if (argc-1 > MAXARG) {
      printf("too much params\n");
      break;
    }
    nargv[argc-1] = buf;
    if ((pid = fork()) == 0) {
      exec(argv[1], nargv);
    } else {
      wait(&status);
    }
    buf[0] = '\0';
  }
  exit(0);
}