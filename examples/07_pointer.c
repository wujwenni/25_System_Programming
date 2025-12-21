/* Pointer Example
 *
 * Demonstrates pointer usage:
 * - Pointer declaration and dereferencing
 * - Passing pointers to functions
 * - Good for testing memory viewer
 */

#include <stdio.h>

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main() {
    int x = 10;
    int y = 20;
    int *ptr_x = &x;
    int *ptr_y = &y;

    printf("Before swap:\n");
    printf("  x = %d, y = %d\n", x, y);
    printf("  *ptr_x = %d, *ptr_y = %d\n", *ptr_x, *ptr_y);

    swap(&x, &y);

    printf("After swap:\n");
    printf("  x = %d, y = %d\n", x, y);
    printf("  *ptr_x = %d, *ptr_y = %d\n", *ptr_x, *ptr_y);

    return 0;
}
