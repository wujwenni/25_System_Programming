/* Recursion Example
 *
 * Demonstrates recursive function calls:
 * - Factorial calculation using recursion
 * - Good for testing call stack display (when implemented)
 * - Step through to see recursive calls
 */

#include <stdio.h>

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int n = 5;
    int result = factorial(n);

    printf("factorial(%d) = %d\n", n, result);
    return 0;
}
