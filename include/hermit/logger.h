#pragma once
#include <stdarg.h>
#include <stdbool.h>

typedef enum {
    HLOG_ERR  = 0,
    HLOG_WARN = 1,
    HLOG_INFO = 2,
    HLOG_DEBG = 3
} LogLevel;

void hlog_setDebug(bool debugon);
void hlog_info(const char* msg, ...);
void hlog_error(const char* msg, ...);
void hlog_warn(const char* msg, ...);
void hlog_debug(const char* msg, ...);