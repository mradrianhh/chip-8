#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <pthread.h>

#define MAX_FILENAME_SIZE 256

typedef enum LogLevel
{
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_EVENT = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5,
    LOG_LEVEL_FULL = 15
} LogLevel;

typedef struct Logger
{
    FILE *file_pointer;
    char *filename;
} Logger;

Logger *logger_init(char *filename);

int set_log_level(LogLevel log_level);

int log_error(const Logger *logger, const char *format, ...);

int log_event(const Logger *logger, const char *format, ...);

int log_info(const Logger * logger, const char *format, ...);

int log_debug(const Logger *logger, const char *format, ...);

int log_trace(const Logger *logger, const char *format, ...);

void logger_destroy(Logger *logger);


#endif
