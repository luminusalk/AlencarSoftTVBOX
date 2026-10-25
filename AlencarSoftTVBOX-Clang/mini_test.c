#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    system("echo MINI_TEST_SUCCESS > /tmp/mini_test.txt");
    write(1, "ALENCARSOFT_MINI_TEST_OK\n", 25);
    return 0;
}
