#include <stdlib.h>
#include <string.h>

#include "events/events.h"

static EventTable event_table;

static int FindEvent(Event **event, const char *event_name);

// Initialize event_table and logger.
void events_Initialize(LogLevel log_level)
{
    event_table.logger = logger_Initialize(LOGS_BASE_PATH "events.log", log_level);

    event_table.root_event = NULL;
    pthread_mutex_init(&event_table.mutex, NULL);
}

void events_Destroy()
{
    logger_Destroy(event_table.logger);
}

// Creates event with event_name.
//
// Events should be created during initializations.
//
// Returns 0 if event created.
// Returns 1 if event with name already exists.
int events_CreateEvent(const char *event_name)
{
    logger_LogInfo(event_table.logger, "Attempting to create event %s.", event_name);

    pthread_mutex_lock(&event_table.mutex);

    // If root is null, we insert as root. RC=0.
    if (event_table.root_event == NULL)
    {
        event_table.root_event = calloc(1, sizeof(Event));
        event_table.root_event->next_event = NULL;
        event_table.root_event->root_subscription = NULL;
        strcpy(event_table.root_event->event_name, event_name);
        logger_LogInfo(event_table.logger, "Create event %s. Succeeded - inserted as root.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 0;
    }

    // Check duplicate name for root. If duplicate, return. RC=1.
    if (!strcmp(event_table.root_event->event_name, event_name))
    {
        logger_LogInfo(event_table.logger, "Create event %s. Failed - an event already exists with that name.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 1;
    }

    // Walk through events until next available spot is found. Also check if an event already exists with that name.
    Event *current = event_table.root_event;
    while (current->next_event != NULL)
    {
        // If an event already exists with that name, it's a duplicate and we return. RC=1.
        if (!strcmp(current->next_event->event_name, event_name))
        {
            logger_LogInfo(event_table.logger, "Create event %s. Failed - an event already exists with that name.", event_name);
            pthread_mutex_unlock(&event_table.mutex);
            return 1;
        }
        current = current->next_event;
    }

    current->next_event = calloc(1, sizeof(Event));
    strcpy(current->next_event->event_name, event_name);
    current->next_event->next_event = NULL;
    current->next_event->root_subscription = NULL;
    logger_LogInfo(event_table.logger, "Create event %s. Succeeded.", event_name);

    pthread_mutex_unlock(&event_table.mutex);
    return 0;
}

// Removes event with event_name.
//
// Returns 0 if event found.
// Returns 1 if table is empty.
// Returns 2 if event not found.
int events_RemoveEvent(const char *event_name)
{
    logger_LogInfo(event_table.logger, "Attempting to remove event %s.", event_name);

    // If root is NULL, there are no events to remove.
    if (event_table.root_event == NULL)
    {
        logger_LogInfo(event_table.logger, "Remove event %s. Failed - _event_table empty.", event_name);
        return 1;
    }

    // If event is root, check if next is not NULL.
    if (!strcmp(event_table.root_event->event_name, event_name))
    {
        pthread_mutex_lock(&event_table.mutex);
        // If next is not NULL, set next as root.
        if (event_table.root_event->next_event != NULL)
        {
            event_table.root_event = event_table.root_event->next_event;
            logger_LogInfo(event_table.logger, "Remove event %s. Succeeded - re-assigned root.", event_name);
        }
        // Else, set root to NULL.
        else
        {
            event_table.root_event = NULL;
            logger_LogInfo(event_table.logger, "Remove event %s. Succeeded - _event_table empty.", event_name);
        }

        pthread_mutex_unlock(&event_table.mutex);
        return 0;
    }

    // Walk through event_table until end or event is found.
    pthread_mutex_lock(&event_table.mutex);
    Event *current = event_table.root_event;
    while (strcmp(current->next_event->event_name, event_name) && current->next_event != NULL)
    {
        current = current->next_event;
    }

    // If event not found, return.
    if (current->next_event == NULL)
    {
        logger_LogInfo(event_table.logger, "Remove event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 2;
    }
    // Else, event is found.

    // If the event's next is not null, point the current's next to the event's next.
    if (current->next_event->next_event != NULL)
    {
        current->next_event = current->next_event->next_event;
    }
    // Else, point the current's next to null.
    else
    {
        current->next_event = NULL;
    }

    pthread_mutex_unlock(&event_table.mutex);
    logger_LogInfo(event_table.logger, "Remove event %s. Succeeded.", event_name);
    return 0;
}

// Trigger event with event_name.
//
// Returns 0 if event found and triggered.
// Returns 1 if table is empty.
// Returns 2 if event not found.
int events_NotifyListeners(const char *event_name, const void *args)
{
    logger_LogInfo(event_table.logger, "Attempting to trigger event %s.", event_name);

    // Find event.
    pthread_mutex_lock(&event_table.mutex);
    Event *event;
    int rc = FindEvent(&event, event_name);
    // TODO: Change from hardcoded integers to enum in testing on RC.
    switch (rc)
    {
    // Event_table empty.
    case 1:
        logger_LogInfo(event_table.logger, "Trigger event %s. Failed - _event_table empty.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return rc;
    // Event not found.
    case 2:
        logger_LogInfo(event_table.logger, "Trigger event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return rc;
    // Event found.
    default:
        break;
    }

    // Walk through subscriptions and call handlers.
    EventSubscription *current = event->root_subscription;
    while (current != NULL)
    {
        current->handler(args);
        current = current->next_subscription;
    }

    logger_LogInfo(event_table.logger, "Trigger event %s. Succeeded.", event_name);
    pthread_mutex_unlock(&event_table.mutex);
    return 0;
}

// Subscribe handler to event with event_name.
//
// Events should not be subscribed to during initializations.
//
// Returns 0 if event found and subscription added.
// Returns 1 if table is empty.
// Returns 2 if event not found.
// Returns 3 if event found but handler already subscribed.
int events_Subscribe(const char *event_name, event_handler handler)
{
    logger_LogInfo(event_table.logger, "Attempting to subscribe to event %s.", event_name);

    // Find event.
    pthread_mutex_lock(&event_table.mutex);
    Event *event;
    int rc = FindEvent(&event, event_name);
    switch (rc)
    {
    // Event_table empty.
    case 1:
        logger_LogInfo(event_table.logger, "Subscribe to event %s. Failed - _event_table empty.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return rc;
    // Event not found.
    case 2:
        logger_LogInfo(event_table.logger, "Subscribe to event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return rc;
    // Event found.
    default:
        break;
    }

    // If root is null, we insert as root. RC=0.
    if (event->root_subscription == NULL)
    {
        event->root_subscription = calloc(1, sizeof(EventSubscription));
        event->root_subscription->handler = handler;
        event->root_subscription->next_subscription = NULL;
        logger_LogInfo(event_table.logger, "Subscribe to event %s. Succeeded - inserted as root.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 0;
    }

    // Check duplicate handler for root. If duplicate, return. RC=3.
    if (event->root_subscription->handler == handler)
    {
        logger_LogInfo(event_table.logger, "Subscribe to event %s. Failed - the handler is already subscribed to the event.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 3;
    }

    // Walk through subscriptions until next available spot is found. Also check if a subscription with the handler already exists.
    EventSubscription *current = event->root_subscription;
    while (current->next_subscription != NULL)
    {
        // If the handler is already subscribed, it's a duplicate and we return. RC=3.
        if (current->next_subscription->handler == handler)
        {
            logger_LogInfo(event_table.logger, "Subscribe to event %s. Failed - the handler is already subscribed to the event.", event_name);
            pthread_mutex_unlock(&event_table.mutex);
            return 3;
        }
        current = current->next_subscription;
    }

    current->next_subscription = calloc(1, sizeof(EventSubscription));
    current->next_subscription->handler = handler;
    current->next_subscription->next_subscription = NULL;
    logger_LogInfo(event_table.logger, "Subscribe to event %s. Succeeded.", event_name);
    pthread_mutex_unlock(&event_table.mutex);
    return 0;
}

// Unsubscribe handler from event with event_name.
//
// Returns 0 if event found and unsubscribed.
// Returns 1 if table is empty.
// Returns 2 if event not found.
// Returns 3 if event found but handler not subscribed.
int events_Unsubscribe(const char *event_name, event_handler handler)
{
    logger_LogInfo(event_table.logger, "Attempting to unsubscribe to event %s.", event_name);

    // Find event.
    pthread_mutex_lock(&event_table.mutex);
    Event *event;
    int rc = FindEvent(&event, event_name);
    switch (rc)
    {
    case 1:
        logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Failed - _event_table empty.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return rc;
    case 2:
        logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return rc;
    default:
        break;
    }

    // If root is NULL, the handler is not subscribed.
    if (event->root_subscription == NULL)
    {
        logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Failed - subscriptions empty.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 3;
    }

    // If handler is subscribed as root, check if next is not NULL.
    if (event->root_subscription->handler == handler)
    {
        // If next is not NULL, set next as root.
        if (event->root_subscription->next_subscription != NULL)
        {
            event->root_subscription = event->root_subscription->next_subscription;
            logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Succeeded - re-assigned root.", event_name);
        }
        // Else, set root to NULL.
        else
        {
            event->root_subscription = NULL;
            logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Succeeded - subscriptions empty.", event_name);
        }

        pthread_mutex_unlock(&event_table.mutex);
        return 0;
    }

    // Walk through subscriptions until end or handler found.
    EventSubscription *current = event->root_subscription;
    while (current->next_subscription != NULL && current->next_subscription->handler != handler)
    {
        current = current->next_subscription;
    }

    // If subscription not found, return.
    if (current->next_subscription == NULL)
    {
        logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Failed - the handler is not subscribed.", event_name);
        pthread_mutex_unlock(&event_table.mutex);
        return 3;
    }
    // Else, subscription is found.

    // If the subscriptions's next is not null, point the current's next to the subscriptions's next.
    if (current->next_subscription->next_subscription != NULL)
    {
        current->next_subscription = current->next_subscription->next_subscription;
    }
    // Else, point the current's next to null.
    else
    {
        current->next_subscription = NULL;
    }

    pthread_mutex_unlock(&event_table.mutex);
    logger_LogInfo(event_table.logger, "Unsubscribe to event %s. Succeeded.", event_name);
    return 0;
}

// Finds event with event_name and stores it in event if found.
//
// NOTE: Mutex must be controlled by caller.
//
// Returns 0 if event found.
// Returns 1 if table is empty.
// Returns 2 if event not found.
int FindEvent(Event **event, const char *event_name)
{
    // If root is NULL, table is empty. RC=1.
    if (event_table.root_event == NULL)
    {
        *event = NULL;
        return 1;
    }

    // If root has event_name, return root. RC=0.
    if (!strcmp(event_table.root_event->event_name, event_name))
    {
        *event = event_table.root_event;
        return 0;
    }

    // Walk through event_table until end or event found.
    Event *current = event_table.root_event;
    while (current->next_event != NULL && (strcmp(current->next_event->event_name, event_name) != 0))
    {
        current = current->next_event;
    }

    // If event not found, return.
    if (current->next_event == NULL)
    {
        event = NULL;
        return 2;
    }

    // Else, event is found. RC=0.
    *event = current->next_event;
    return 0;
}
