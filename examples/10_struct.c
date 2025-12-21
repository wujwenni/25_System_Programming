/* Struct Example
 *
 * Demonstrates structure usage:
 * - Define and use custom data types
 * - Access struct members
 * - Good for testing variable viewer with complex types
 */

#include <stdio.h>
#include <string.h>

struct Student {
    char name[50];
    int age;
    float gpa;
};

void print_student(struct Student s) {
    printf("Name: %s\n", s.name);
    printf("Age: %d\n", s.age);
    printf("GPA: %.2f\n", s.gpa);
}

int main() {
    struct Student student1;

    strcpy(student1.name, "Alice");
    student1.age = 20;
    student1.gpa = 3.75;

    printf("Student Information:\n");
    print_student(student1);

    return 0;
}
