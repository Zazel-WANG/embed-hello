#include "hello.h"
#include <stdio.h>
#include <string.h>

#if defined(__aarch64__)
const char* hello_arch(void) { return "aarch64 (ARM64)"; }
#elif defined(__x86_64__)
const char* hello_arch(void) { return "x86_64"; }
#else
const char* hello_arch(void) { return "unknown"; }
#endif

static int test_arch_detection(void) {
    const char* arch = hello_arch();
    if (strlen(arch) == 0) return 0;
    return 1;
}

static int test_factorial(void) {
    int n = 5, expected = 120, result = 1;
    for (int i = 1; i <= n; i++) result *= i;
    return result == expected;
}

int main(void) {
    int passed = 0, failed = 0;

    printf("=== embed-hello Unit Tests ===\n\n");

    if (test_arch_detection()) {
        printf("  [TEST] arch detection ... PASS (%s)\n", hello_arch());
        passed++;
    } else {
        printf("  [TEST] arch detection ... FAIL\n");
        failed++;
    }

    if (test_factorial()) {
        printf("  [TEST] factorial n=5 ... PASS\n");
        passed++;
    } else {
        printf("  [TEST] factorial n=5 ... FAIL\n");
        failed++;
    }

    printf("\n=== 总计: %d  通过: %d  失败: %d ===\n", passed + failed, passed, failed);
    return failed > 0 ? 1 : 0;
}
