#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int modi_strcmp(const char *s1, char *s2) {
  char *p;
  int index = 0;
  while (s2[index] != 0) {
    index++;
  }
  while (s2[index] != '/') {
    index--;
  }
  p = &s2[index+1];
  return strcmp(s1, p);
}

void fs_recursion(char *path, char *target) {
  int fd;
  char *p, buf[512];
  struct dirent de;
  struct stat st;
  if ((fd = open(path, 0)) < 0) {
    printf("can't open %s\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    printf("can't stat %s\n", path);
    close(fd);
    return;
  }
  switch (st.type) {
  case T_FILE:
    if (modi_strcmp(target, path) == 0) {
      printf("%s\n", path);
    }
    break;
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
        continue;
      }
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      fs_recursion(buf, target);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("usage:\nfind <path> <filename>\n");
    exit(-1);
  }
  fs_recursion(argv[1], argv[2]);
  exit(0);
}