#ifndef HYPER_POC_PRINTF_H
#define HYPER_POC_PRINTF_H

int printf(const char *fmt, ...);
int printf_debug(int level, const char *fmt, ...);
void panic(const char *fmt, ...) __attribute__((noreturn));

#endif
