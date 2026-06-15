#include "hello.h"

#if defined(__aarch64__)
const char* hello_arch(void) { return "aarch64 (ARM64)"; }
#elif defined(__x86_64__)
const char* hello_arch(void) { return "x86_64"; }
#else
const char* hello_arch(void) { return "unknown"; }
#endif
