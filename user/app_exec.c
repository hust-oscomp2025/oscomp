#include <stdio.h>

int main(int argc, char *argv[]) {
	char a[10];
	int* i = (int*)&a[1];
	*i = 10086;
  printf("Hello World!\n");

  return 0;
}
