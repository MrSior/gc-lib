#include "gc/log.h"

void LOG_PRINTF(const char *format, ...) {
    va_list args;
    va_start(args, format);

    printf("\033[90m[LOG]:\033[0m ");

    const char *p = format;
    while (*p) {
        if (*p == '%' && *(p + 1) == 'p') {
            printf("\033[32m");
            void *ptr = va_arg(args, void *);
            printf("%p", ptr);
            printf("\033[0m");
            p += 2;
        } else {
            if (*p == '%') {
                vprintf(p, args);
                break;
            } else {
                putchar(*p);
                p++;
            }
        }
    }

    va_end(args);
    printf("\n");
}