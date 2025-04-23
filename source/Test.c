
#include <stdio.h>
#include <time.h>

#define LDBUG 0 // DEBUG
#define LINF 1  // INFO
#define LERR 2  // ERR
#define DEFAULT LDBUG

#define LOG(level, format, ...)                                                             \
    {                                                                                       \
        if (level >= DEFAULT)                                                               \
        {                                                                                   \
            time_t t = time(NULL);                                                          \
            struct tm *lt = localtime(&t) char timetmp[32];                                 \
            strftime(timetmp, 31, "%m-%d %T", lt);                                          \
            printf("[%s][%s:%d]" format "\n", timetmp, __FIFL__, __LINE__, ##__VA__ARGS__); \
        }                                                                                   \
    }

#define DLOG(format, ...) LOG(LDBUG, format, ##__VA__ARGS__);
#define DLOG(format, ...) LOG(LINF, format, ##__VA__ARGS__);
#define DLOG(format, ...) LOG(LERR, format, ##__VA__ARGS__);
