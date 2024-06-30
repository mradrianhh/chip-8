#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <pthread.h>

#ifdef CH8_LOGS_DIR
#define LOGS_BASE_PATH CH8_LOGS_DIR
#else
#define LOGS_BASE_PATH "../../logs/"
#endif

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
    LogLevel log_level;
} Logger;

Logger *logger_Initialize(char *filename, LogLevel log_level);

int logger_SetLogLevel(Logger *logger, LogLevel log_level);

int logger_LogError(const Logger *logger, const char *format, ...);

int logger_LogEvent(const Logger *logger, const char *format, ...);

int logger_LogInfo(const Logger * logger, const char *format, ...);

int logger_LogDebug(const Logger *logger, const char *format, ...);

int logger_LogTrace(const Logger *logger, const char *format, ...);

void logger_Destroy(Logger *logger);


#endif
