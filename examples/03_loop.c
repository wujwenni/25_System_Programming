/* Loop Example
 *
 * Demonstrates debugging loops:
 * - Use (n) to step through each iteration
 * - Watch the loop counter change
 * - Good for testing breakpoints (when implemented)
 */

#include <stdio.h>

int main() {
    printf("Counting from 1 to 5:\n");

    for (int i = 1; i <= 5; i++) {
        printf("  i = %d\n", i);
    }

    printf("Done!\n");
    return 0;
}
