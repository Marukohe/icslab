#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void init_rand() {
  srand(time(0));
}

static inline int choose(int n) { return rand() % n; }

void die(void) {
  int *p = NULL;
  printf("%d", *p);
}

void recursive(int n, int m, int p, int q) {
  if (n + m + p + q == 50) return;
  else {
    switch (choose(16)) {
      case 0: die(); break;
      case 1 ... 4: recursive(n + 1, m, p, q); break;
      case 5 ... 8: recursive(n, m + 1, p, q); break;
      case 9 ... 12: recursive(n, m, p + 1, q); break;
      case 13 ... 15: recursive(n, m, p, q + 1); break;
    }
  }
}

int main() {
  printf("This is %s\n", __FILE__);

  init_rand();

  recursive(0, 0, 0, 0);
  die();

  return 0;
}
