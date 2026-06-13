#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "hermit/logger.h"

static bool debug;

void hlog_setDebug(bool debugon) {
    debug = debugon;
    // This is kinda a stupid way to do this, but i might add more init stuff here who knows. 
}

static void func_log(LogLevel lvl, const char* msg, va_list args) {
    const char* lvl_clr[4] = {"1;31", "1;33", "1;32", "1;34"};
    const char* lvl_str[4] = {"[ERROR]: ", "[WARNING]: ", "[INFO]: ", "[DEBUG]: "};
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), msg, args);
    if ((debug == true) || (debug == false && lvl<3)) {
        printf("\033[%sm%s%s\033[0m\n", lvl_clr[lvl], lvl_str[lvl], buffer);
        fflush(stdout);        
    }
}

void hlog_error(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    func_log(HLOG_ERR, msg, args);
    va_end(args);
}

void hlog_warn(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    func_log(HLOG_WARN, msg, args);
    va_end(args);
}

void hlog_info(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    func_log(HLOG_INFO, msg, args);
    va_end(args);
}

void hlog_debug(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    func_log(HLOG_DEBG, msg, args);
    va_end(args);
}