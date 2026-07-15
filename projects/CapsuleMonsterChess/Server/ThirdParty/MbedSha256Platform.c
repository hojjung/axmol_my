#include <stddef.h>

void mbedtls_platform_zeroize(void* buffer, size_t size)
{
    volatile unsigned char* current = (volatile unsigned char*)buffer;
    while (size-- > 0)
        *current++ = 0;
}
