#include <stdio.h>

#define N 1

int add(int a, int b) {
  int c = a + b;
  return c;
}

int main() {
  printf("Hello World!\n");
  printf("%d + %d = %d\n", N, N, add(N, N));
  return 0;
}
