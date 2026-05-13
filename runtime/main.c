#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

extern int64_t compiled_fn(int64_t arg);

int main() {
    int64_t result = compiled_fn(10);
    printf("%ld\n", result);
    return 0;
}
