#pragma once
#include <stdio.h>
#include <time.h>

#define LDBG 0
#define LINF 1
#define LERR 2
#define LDEFAULT LINF

#define LOG(level, format, ...)                                                                       \
    {                                                                                                 \
        if (level >= LDEFAULT)                                                                        \
        {                                                                                             \
            time_t t = time(NULL);                                                                    \
            struct tm *lt = localtime(&t);                                                            \
            char time_tmp[32] = {0};                                                                  \
            strftime(time_tmp, 31, "%m-%d %T", lt);                                                   \
            fprintf(stdout, "[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                                             \
    }

#define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__);
#define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__);
#define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__);
