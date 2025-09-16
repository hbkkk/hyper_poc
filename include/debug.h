#ifndef DEBUG_H
#define DEBUG_H

#include "printf.h"

#define LOG_LEVEL_NONE          0
#define LOG_LEVEL_ERROR         1
#define LOG_LEVEL_NOTICE        2
#define LOG_LEVEL_WARNING       3
#define LOG_LEVEL_TRACE         4
#define LOG_LEVEL_INFO          5

# define LOG_ERR(_f, ...)    printf_debug(LOG_LEVEL_ERROR, "[HYP-ERR] "_f, ##__VA_ARGS__)
# define LOG_NOTICE(_f, ...) printf_debug(LOG_LEVEL_NOTICE, "[HYP-NTC] "_f, ##__VA_ARGS__)
# define LOG_WARN(_f, ...)   printf_debug(LOG_LEVEL_WARNING, "[HYP-WAR] "_f, ##__VA_ARGS__)
# define LOG_TRACE(_f, ...)  printf_debug(LOG_LEVEL_TRACE, _f, ##__VA_ARGS__)
# define LOG_INFO(_f, ...)   printf_debug(LOG_LEVEL_INFO, _f, ##__VA_ARGS__)

#endif