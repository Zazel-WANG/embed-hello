#include "hello.h"
#include <stdio.h>

int main(void) {
    printf("Hello from ARM64!\n");
    printf("Architecture: %s\n", hello_arch());
    printf("Build: embed-hello v1.0\n");
    return 0;
}
