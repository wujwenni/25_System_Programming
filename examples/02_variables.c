/* Variable Example
 *
 * Demonstrates variable initialization and arithmetic:
 * - Step through (n) and observe variables in registers
 * - Watch how values change at each step
 */

#include <stdio.h>

int main() {
    int x = 10;
    int y = 20;
    int sum = x + y;
    int product = x * y;

    printf("x = %d\n", x);
    printf("y = %d\n", y);
    printf("sum = %d\n", sum);
    printf("product = %d\n", product);

    return 0;
}
