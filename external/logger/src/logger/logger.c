#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

#include "logger/logger.h"

static int LogWrite(const Logger *logger, const char *log_level, const char *format, va_list args);
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

Logger *logger_Initialize(char *filename, LogLevel log_level)
{
    Logger *logger = calloc(1, sizeof(Logger));
    logger->filename = realloc(logger->filename, strlen(filename));
    strcpy(logger->filename, filename);
    logger->log_level = log_level;

    if ((logger->file_pointer = fopen(logger->filename, "w")) == NULL)
    {
        printf("Internal log error: Can't open file '%s'.\n", logger->filename);
        raise(SIGABRT);
    }

    return logger;
}

int logger_SetLogLevel(Logger *logger, LogLevel log_level)
{
    logger->log_level = log_level;
    return 0;
}

int logger_LogError(const Logger *logger, const char *format, ...)
{
    if (logger->log_level < LOG_LEVEL_ERROR)
    {
        return 0;
    }

    va_list args;
    va_start(args, format);
    LogWrite(logger, "ERROR", format, args);
    va_end(args);

    return 0;
}

int logger_LogEvent(const Logger *logger, const char *format, ...)
{
    if (logger->log_level < LOG_LEVEL_EVENT)
    {
        return 0;
    }

    va_list args;
    va_start(args, format);
    LogWrite(logger, "EVENT", format, args);
    va_end(args);

    return 0;
}

int logger_LogInfo(const Logger *logger, const char *format, ...)
{
    if (logger->log_level < LOG_LEVEL_INFO)
    {
        return 0;
    }

    va_list args;
    va_start(args, format);
    LogWrite(logger, "INFO", format, args);
    va_end(args);

    return 0;
}

int logger_LogDebug(const Logger *logger, const char *format, ...)
{
    if (logger->log_level < LOG_LEVEL_DEBUG)
    {
        return 0;
    }

    va_list args;
    va_start(args, format);
    LogWrite(logger, "DEBUG", format, args);
    va_end(args);

    return 0;
}

int logger_LogTrace(const Logger *logger, const char *format, ...)
{
    if (logger->log_level < LOG_LEVEL_TRACE)
    {
        return 0;
    }

    va_list args;
    va_start(args, format);
    LogWrite(logger, "TRACE", format, args);
    va_end(args);

    return 0;
}

void logger_Destroy(Logger *logger)
{
    if(fclose(logger->file_pointer))
    {
        printf("Internal log error: Can't close file '%s'.\n", logger->filename);
        raise(SIGABRT);
    }
    free(logger->filename);
    free(logger);
}

static int LogWrite(const Logger *logger, const char *log_level, const char *format, va_list args)
{
    pthread_mutex_lock(&_mutex);
    int rc = 0;

    struct tm *timer_fmt;
    time_t timer;
    time(&timer);
    timer_fmt = gmtime(&timer);

    fprintf(logger->file_pointer, "[UTC: %02d-%02d-%04d %02d-%02d-%02d] - %s: ", timer_fmt->tm_mday, (timer_fmt->tm_mon + 1), (timer_fmt->tm_year + 1900),
                timer_fmt->tm_hour, timer_fmt->tm_min, timer_fmt->tm_sec, log_level);
    vfprintf(logger->file_pointer, format, args);
    fprintf(logger->file_pointer, "\n");

    fflush(logger->file_pointer);
    pthread_mutex_unlock(&_mutex);

    return 0;
}
