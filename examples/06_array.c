/* Array Example
 *
 * Demonstrates array manipulation:
 * - Initialize and iterate through arrays
 * - Calculate sum and average
 * - Good for testing memory viewing (when implemented)
 */

#include <stdio.h>

int main() {
    int numbers[5] = {10, 20, 30, 40, 50};
    int sum = 0;

    printf("Array contents:\n");
    for (int i = 0; i < 5; i++) {
        printf("  numbers[%d] = %d\n", i, numbers[i]);
        sum += numbers[i];
    }

    double average = sum / 5.0;
    printf("Sum: %d\n", sum);
    printf("Average: %.1f\n", average);

    return 0;
}
