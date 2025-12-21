/* Fibonacci Sequence Example
 *
 * Demonstrates iterative algorithm:
 * - Fibonacci number calculation
 * - Good for stepping through algorithm logic
 * - Watch how variables change in each iteration
 */

#include <stdio.h>

int main() {
    int n = 10;
    int a = 0, b = 1, next;

    printf("Fibonacci sequence (first %d numbers):\n", n);
    printf("%d ", a);
    printf("%d ", b);

    for (int i = 2; i < n; i++) {
        next = a + b;
        printf("%d ", next);
        a = b;
        b = next;
    }

    printf("\n");
    return 0;
}
