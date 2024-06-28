#include <stdlib.h>
#include <string.h>

#include "events.h"

static _EventTable_t _event_table;

static int find_event(_Event_t **event, const char *event_name);

// Initialize event_table and logger.
void events_init()
{
    _event_table._logger = logger_init(LOGS_BASE_PATH "events.log");
    log_info(_event_table._logger, "Initializing.");

    _event_table._root_event = NULL;
    pthread_mutex_init(&_event_table._mutex, NULL);
}

void events_shutdown()
{
    log_info(_event_table._logger, "Shutting down.");
    logger_destroy(_event_table._logger);
}

// Creates event with event_name.
//
// Events should be created during initializations.
//
// Returns 0 if event created.
// Returns 1 if event with name already exists.
int event_create(const char *event_name)
{
    log_info(_event_table._logger, "Attempting to create event %s.", event_name);

    pthread_mutex_lock(&_event_table._mutex);

    // If root is null, we insert as root. RC=0.
    if (_event_table._root_event == NULL)
    {
        _event_table._root_event = (_Event_t *)malloc(sizeof(_Event_t));
        _event_table._root_event->_next_event = NULL;
        _event_table._root_event->_root_subscription = NULL;
        strcpy(_event_table._root_event->event_name, event_name);
        log_info(_event_table._logger, "Create event %s. Succeeded - inserted as root.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 0;
    }

    // Check duplicate name for root. If duplicate, return. RC=1.
    if (!strcmp(_event_table._root_event->event_name, event_name))
    {
        log_info(_event_table._logger, "Create event %s. Failed - an event already exists with that name.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 1;
    }

    // Walk through events until next available spot is found. Also check if an event already exists with that name.
    _Event_t *current = _event_table._root_event;
    while (current->_next_event != NULL)
    {
        // If an event already exists with that name, it's a duplicate and we return. RC=1.
        if (!strcmp(current->_next_event->event_name, event_name))
        {
            log_info(_event_table._logger, "Create event %s. Failed - an event already exists with that name.", event_name);
            pthread_mutex_unlock(&_event_table._mutex);
            return 1;
        }
        current = current->_next_event;
    }

    current->_next_event = (_Event_t *)malloc(sizeof(_Event_t));
    strcpy(current->_next_event->event_name, event_name);
    current->_next_event->_next_event = NULL;
    current->_next_event->_root_subscription = NULL;
    log_info(_event_table._logger, "Create event %s. Succeeded.", event_name);

    pthread_mutex_unlock(&_event_table._mutex);
    return 0;
}

// Removes event with event_name.
//
// Returns 0 if event found.
// Returns 1 if table is empty.
// Returns 2 if event not found.
int event_remove(const char *event_name)
{
    log_info(_event_table._logger, "Attempting to remove event %s.", event_name);

    // If root is NULL, there are no events to remove.
    if (_event_table._root_event == NULL)
    {
        log_info(_event_table._logger, "Remove event %s. Failed - _event_table empty.", event_name);
        return 1;
    }

    // If event is root, check if next is not NULL.
    if (!strcmp(_event_table._root_event->event_name, event_name))
    {
        pthread_mutex_lock(&_event_table._mutex);
        // If next is not NULL, set next as root.
        if (_event_table._root_event->_next_event != NULL)
        {
            _event_table._root_event = _event_table._root_event->_next_event;
            log_info(_event_table._logger, "Remove event %s. Succeeded - re-assigned root.", event_name);
        }
        // Else, set root to NULL.
        else
        {
            _event_table._root_event = NULL;
            log_info(_event_table._logger, "Remove event %s. Succeeded - _event_table empty.", event_name);
        }

        pthread_mutex_unlock(&_event_table._mutex);
        return 0;
    }

    // Walk through event_table until end or event is found.
    pthread_mutex_lock(&_event_table._mutex);
    _Event_t *current = _event_table._root_event;
    while (strcmp(current->_next_event->event_name, event_name) && current->_next_event != NULL)
    {
        current = current->_next_event;
    }

    // If event not found, return.
    if (current->_next_event == NULL)
    {
        log_info(_event_table._logger, "Remove event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 2;
    }
    // Else, event is found.

    // If the event's next is not null, point the current's next to the event's next.
    if (current->_next_event->_next_event != NULL)
    {
        current->_next_event = current->_next_event->_next_event;
    }
    // Else, point the current's next to null.
    else
    {
        current->_next_event = NULL;
    }

    pthread_mutex_unlock(&_event_table._mutex);
    log_info(_event_table._logger, "Remove event %s. Succeeded.", event_name);
    return 0;
}

// Trigger event with event_name.
//
// Returns 0 if event found and triggered.
// Returns 1 if table is empty.
// Returns 2 if event not found.
int event_notify(const char *event_name, const void *args)
{
    log_info(_event_table._logger, "Attempting to trigger event %s.", event_name);

    // Find event.
    pthread_mutex_lock(&_event_table._mutex);
    _Event_t *event;
    int rc = find_event(&event, event_name);
    switch (rc)
    {
    // Event_table empty.
    case 1:
        log_info(_event_table._logger, "Trigger event %s. Failed - _event_table empty.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return rc;
    // Event not found.
    case 2:
        log_info(_event_table._logger, "Trigger event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return rc;
    // Event found.
    default:
        break;
    }

    // Walk through subscriptions and call handlers.
    _EventSubscription_t *current = event->_root_subscription;
    while (current != NULL)
    {
        current->_handler(args);
        current = current->_next_subscription;
    }

    log_info(_event_table._logger, "Trigger event %s. Succeeded.", event_name);
    pthread_mutex_unlock(&_event_table._mutex);
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
int event_subscribe(const char *event_name, event_handler handler)
{
    log_info(_event_table._logger, "Attempting to subscribe to event %s.", event_name);

    // Find event.
    pthread_mutex_lock(&_event_table._mutex);
    _Event_t *event;
    int rc = find_event(&event, event_name);
    switch (rc)
    {
    // Event_table empty.
    case 1:
        log_info(_event_table._logger, "Subscribe to event %s. Failed - _event_table empty.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return rc;
    // Event not found.
    case 2:
        log_info(_event_table._logger, "Subscribe to event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return rc;
    // Event found.
    default:
        break;
    }

    // If root is null, we insert as root. RC=0.
    if (event->_root_subscription == NULL)
    {
        event->_root_subscription = (_EventSubscription_t *)malloc(sizeof(_EventSubscription_t));
        event->_root_subscription->_handler = handler;
        event->_root_subscription->_next_subscription = NULL;
        log_info(_event_table._logger, "Subscribe to event %s. Succeeded - inserted as root.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 0;
    }

    // Check duplicate handler for root. If duplicate, return. RC=3.
    if (event->_root_subscription->_handler == handler)
    {
        log_info(_event_table._logger, "Subscribe to event %s. Failed - the handler is already subscribed to the event.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 3;
    }

    // Walk through subscriptions until next available spot is found. Also check if a subscription with the handler already exists.
    _EventSubscription_t *current = event->_root_subscription;
    while (current->_next_subscription != NULL)
    {
        // If the handler is already subscribed, it's a duplicate and we return. RC=3.
        if (current->_next_subscription->_handler == handler)
        {
            log_info(_event_table._logger, "Subscribe to event %s. Failed - the handler is already subscribed to the event.", event_name);
            pthread_mutex_unlock(&_event_table._mutex);
            return 3;
        }
        current = current->_next_subscription;
    }

    current->_next_subscription = (_EventSubscription_t *)malloc(sizeof(_EventSubscription_t));
    current->_next_subscription->_handler = handler;
    current->_next_subscription->_next_subscription = NULL;
    log_info(_event_table._logger, "Subscribe to event %s. Succeeded.", event_name);
    pthread_mutex_unlock(&_event_table._mutex);
    return 0;
}

// Unsubscribe handler from event with event_name.
//
// Returns 0 if event found and unsubscribed.
// Returns 1 if table is empty.
// Returns 2 if event not found.
// Returns 3 if event found but handler not subscribed.
int event_unsubscribe(const char *event_name, event_handler handler)
{
    log_info(_event_table._logger, "Attempting to unsubscribe to event %s.", event_name);

    // Find event.
    pthread_mutex_lock(&_event_table._mutex);
    _Event_t *event;
    int rc = find_event(&event, event_name);
    switch (rc)
    {
    case 1:
        log_info(_event_table._logger, "Unsubscribe to event %s. Failed - _event_table empty.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return rc;
    case 2:
        log_info(_event_table._logger, "Unsubscribe to event %s. Failed - event not found.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return rc;
    default:
        break;
    }

    // If root is NULL, the handler is not subscribed.
    if (event->_root_subscription == NULL)
    {
        log_info(_event_table._logger, "Unsubscribe to event %s. Failed - subscriptions empty.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 3;
    }

    // If handler is subscribed as root, check if next is not NULL.
    if (event->_root_subscription->_handler == handler)
    {
        // If next is not NULL, set next as root.
        if (event->_root_subscription->_next_subscription != NULL)
        {
            event->_root_subscription = event->_root_subscription->_next_subscription;
            log_info(_event_table._logger, "Unsubscribe to event %s. Succeeded - re-assigned root.", event_name);
        }
        // Else, set root to NULL.
        else
        {
            event->_root_subscription = NULL;
            log_info(_event_table._logger, "Unsubscribe to event %s. Succeeded - subscriptions empty.", event_name);
        }

        pthread_mutex_unlock(&_event_table._mutex);
        return 0;
    }

    // Walk through subscriptions until end or handler found.
    _EventSubscription_t *current = event->_root_subscription;
    while (current->_next_subscription != NULL && current->_next_subscription->_handler != handler)
    {
        current = current->_next_subscription;
    }

    // If subscription not found, return.
    if (current->_next_subscription == NULL)
    {
        log_info(_event_table._logger, "Unsubscribe to event %s. Failed - the handler is not subscribed.", event_name);
        pthread_mutex_unlock(&_event_table._mutex);
        return 3;
    }
    // Else, subscription is found.

    // If the subscriptions's next is not null, point the current's next to the subscriptions's next.
    if (current->_next_subscription->_next_subscription != NULL)
    {
        current->_next_subscription = current->_next_subscription->_next_subscription;
    }
    // Else, point the current's next to null.
    else
    {
        current->_next_subscription = NULL;
    }

    pthread_mutex_unlock(&_event_table._mutex);
    log_info(_event_table._logger, "Unsubscribe to event %s. Succeeded.", event_name);
    return 0;
}

// Finds event with event_name and stores it in event if found.
//
// NOTE: Mutex must be controlled by caller.
//
// Returns 0 if event found.
// Returns 1 if table is empty.
// Returns 2 if event not found.
int find_event(_Event_t **event, const char *event_name)
{
    // If root is NULL, table is empty. RC=1.
    if (_event_table._root_event == NULL)
    {
        *event = NULL;
        return 1;
    }

    // If root has event_name, return root. RC=0.
    if (!strcmp(_event_table._root_event->event_name, event_name))
    {
        *event = _event_table._root_event;
        return 0;
    }

    // Walk through event_table until end or event found.
    _Event_t *current = _event_table._root_event;
    while (current->_next_event != NULL && (strcmp(current->_next_event->event_name, event_name) != 0))
    {
        current = current->_next_event;
    }

    // If event not found, return.
    if (current->_next_event == NULL)
    {
        event = NULL;
        return 2;
    }

    // Else, event is found. RC=0.
    *event = current->_next_event;
    return 0;
}
