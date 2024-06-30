#ifndef EVENTS_H
#define EVENTS_H

#ifndef MAX_EVENT_SUBSCRIPTIONS
#define MAX_EVENT_SUBSCRIPTIONS         (10)
#endif

#ifndef MAX_EVENT_SUBSCRIPTIONS_SIZE
#define MAX_EVENT_SUBSCRIPTIONS_SIZE    (sizeof(EventSubscription) * MAX_EVENT_SUBSCRIPTIONS)
#endif

#ifndef MAX_EVENT_NAME_SIZE
#define MAX_EVENT_NAME_SIZE             (50)
#endif

#include <stdint.h>
#include <pthread.h>

#include "logger/logger.h"

typedef void(*event_handler)(const void *args);

typedef struct Event Event;
typedef struct EventTable EventTable;
typedef struct EventSubscription EventSubscription;

struct Event
{
    char event_name[MAX_EVENT_NAME_SIZE];
    Event *next_event;
    EventSubscription *root_subscription;
};

struct EventTable
{
    Event *root_event;
    Logger *logger;
    pthread_mutex_t mutex;
};

struct EventSubscription
{
    EventSubscription *next_subscription;
    event_handler handler;
};

void events_Initialize(LogLevel log_level);

void events_Destroy();

int events_CreateEvent(const char *event_name);

int events_RemoveEvent(const char *event_name);

int events_NotifyListeners(const char *event_name, const void *args);

int events_Subscribe(const char *event_name, event_handler handler);

int events_Unsubscribe(const char *event_name, event_handler handler);

#endif
