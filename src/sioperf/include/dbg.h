#ifndef __DBG_H__
#define __DBG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef DEBUG_BUILD
    #define DEBUG(M, ...) fprintf(stderr, "[DEBUG] %s:%3d: " M, __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG(M, ...)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define LOG_ERR(M, ...) fprintf(stdout, "[ERROR] %s:%d: " M, __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG_WARN(M, ...) fprintf(stdout, "[WARN]  %s:%d: " M, __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG_INFO(M, ...) fprintf(stdout, "[INFO]  %s:%d: " M, __FILE__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) { LOG_ERR(M, ##__VA_ARGS__); errno=0; goto error; }

#define sentinel(M, ...)  { LOG_ERR(M, ##__VA_ARGS__); errno=0; goto error; }

#define check_mem(A) check((A), "Out of memory.")

#define check_debug(A, M, ...) if(!(A)) { DEBUG(M, ##__VA_ARGS__); errno=0; goto error; }

#endif