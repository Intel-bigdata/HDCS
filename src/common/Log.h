#ifndef LOG_H
#define LOG_H

#include <string>
#include <time.h>
#include <string.h>
#include <thread>
#include <iostream>
#include <stdarg.h>
#include <stdio.h>

#define BUF_SIZE 256
#define LOG_PRINT_ENABLE 0

namespace hdcs {
static bool LOG_DEBUG_ENABLE = false;
static FILE* log_fd = stderr;

static int timespec2str(char *buf, uint16_t len, struct timespec *ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return 1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0)
        return 2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len)
        return 3;

    return 0;
}


static void get_time(char* time_str)
{
    const uint16_t TIME_FMT = strlen("2012-12-31 12:59:59.123456789") + 1;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    timespec2str( time_str, TIME_FMT, &spec);
}
static void log_err( const char* output, ... ){
    char *time_str = (char*)malloc( sizeof(char) * BUF_SIZE );
    char *buf = (char*)malloc( sizeof(char) * BUF_SIZE );
    va_list vl;
    va_start(vl, output);
    vsnprintf( buf, BUF_SIZE, output, vl);
    va_end(vl);
    get_time(time_str);
    fprintf( log_fd, "[%s] %s\n", time_str, buf );
    free( time_str );
    free( buf );
}

static void log_print( const char* output, ... ){
    if( !LOG_PRINT_ENABLE )
        return;

    char *time_str = (char*)malloc( sizeof(char) * BUF_SIZE );
    char *buf = (char*)malloc( sizeof(char) * BUF_SIZE );
    va_list vl;
    va_start(vl, output);
    vsnprintf( buf, BUF_SIZE, output, vl);
    va_end(vl);
    get_time(time_str);
    fprintf( log_fd, "[%s] %s\n", time_str, buf );
    free( time_str );
    free( buf );
}

static void log_debug( const char* output, ... ){
    if( !LOG_DEBUG_ENABLE )
        return;

    char *time_str = (char*)malloc( sizeof(char) * BUF_SIZE );
    char *buf = (char*)malloc( sizeof(char) * BUF_SIZE );
    va_list vl;
    va_start(vl, output);
    vsnprintf( buf, BUF_SIZE, output, vl);
    va_end(vl);
    get_time(time_str);
    fprintf( log_fd, "[%s] %s\n", time_str, buf );
    free( time_str );
    free( buf );
}

}
#endif
