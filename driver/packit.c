#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>


int main() {
  /*
  char filename[] = "/dev/sdsu0";

  int fp = open(filename, 0);
  if (fp<0) {
    printf("Open %s failed\n", filename);
    return 1;
  }
  */
  char cmd[256];
  char rep[256];
  int n = 0;

  while (!feof(stdin)) {
    int d = 0;
    if (1==scanf("%x", &d)) {
      cmd[n++] = (char)d;
      printf("%c", (char)d);
    }
    else break;
  }
  
  if (n!=16) {
    fprintf(stderr, "n!=16. Ha ha.\n");
    return 1;
  }
  /*
  n = write(fp, cmd, 16);
  printf("Write returned n=%i : %i\n", n, -n);

  if (n>0) {
    n = read(fp, rep, 16);

    printf("Read returned n=%i : %i\n", n, -n);
  } else {
    printf("No read attempted\n");
  }

  close(fp);
  */
  return 0;
}
