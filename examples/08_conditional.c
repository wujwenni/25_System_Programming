/* Conditional Example
 *
 * Demonstrates if-else statements:
 * - Different execution paths
 * - Good for testing breakpoints on specific branches
 */

#include <stdio.h>

int main() {
    int score = 85;

    printf("Score: %d\n", score);

    if (score >= 90) {
        printf("Grade: A\n");
    } else if (score >= 80) {
        printf("Grade: B\n");
    } else if (score >= 70) {
        printf("Grade: C\n");
    } else if (score >= 60) {
        printf("Grade: D\n");
    } else {
        printf("Grade: F\n");
    }

    printf("Result: %s\n", score >= 60 ? "Pass" : "Fail");

    return 0;
}
