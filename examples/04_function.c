/* Function Call Example
 *
 * Demonstrates function calls:
 * - Use (n) to step over the function
 * - Use (s) to step into the function (when implemented)
 * - Observe how execution flows between functions
 */

#include <stdio.h>

int add(int a, int b) {
    int result = a + b;
    return result;
}

int multiply(int a, int b) {
    int result = a * b;
    return result;
}

int main() {
    int x = 5;
    int y = 3;

    int sum = add(x, y);
    printf("add(%d, %d) = %d\n", x, y, sum);

    int product = multiply(x, y);
    printf("multiply(%d, %d) = %d\n", x, y, product);

    return 0;
}
