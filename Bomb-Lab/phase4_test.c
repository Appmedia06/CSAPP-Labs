#include <stdio.h>

int func4(int i, int j, int n)
{
	int avg = (i + j) / 2; // 7
	if (avg <= n) {
		if (avg >= n) {
			return 0;
		}
		else {
			return (2 * func4(avg + 1, j, n)) + 1;
		}
	}
	else {
		return 2 * func4(i, avg - 1, n);
	}
}

int main(void)
{
    int i = 0, j = 14;
    for (int n = 0; n <= 14; n++) {
        printf("n = %d, func4 = %d\n", n, func4(i, j, n));
    }
    return 0;
}
