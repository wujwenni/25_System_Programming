#include <stdio.h>
#include <unistd.h>

int main() {
   printf("main reached\n");
   int x = 5;
   int y = 0;
   int z;
   
   printf("y = %d\n", y);
   printf("y = %d\n", y);
   printf("y = %d\n", y);
   printf("y = %d\n", y);
   printf("y = %d\n", y);
   printf("y = %d\n", y);
   printf("Dummy dummy dummy dummy Dummy dummy dummyDummy dummy dummydummyDummy dummy dummyDummy dummy dummyDummy dummy dummyDummy dummy dummyDummy dummy dummyDummy dummy dummyDummy dummy dummyDummy dummy dummyDummy dummy dummy\n");
   while (y < 5) {
      y++;
      printf("y = %d\n", y);
   }
   printf("x = %d\n", x);
   return 0;
}
