#include <stdio.h>

int main(void) {
	int a = 5, b = 10;
	int temp = a;
	a = b;
	b = temp;

	printf("a = %d, b = %d\n", a, b);
	return 0;
}
