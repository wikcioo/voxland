#include "event.h"

#include "common/log.h"
#include "common/asserts.h"
#include "common/collections/darray.h"

typedef struct registered_event {
    pfn_event_callback *callbacks;
} registered_event_t;

registered_event_t registered_events[NUM_OF_EVENT_CODES];

bool event_system_init(void)
{
    return true;
}

void event_system_shutdown(void)
{
    for (i32 i = 0; i < NUM_OF_EVENT_CODES; i++) {
        if (registered_events[i].callbacks != 0) {
            darray_destroy(registered_events[i].callbacks);
        }
    }
}

void event_system_register(Event_Code code, pfn_event_callback callback)
{
    ASSERT(code > EVENT_CODE_NONE && code < NUM_OF_EVENT_CODES);
    ASSERT(callback);

    if (registered_events[code].callbacks == 0) {
        registered_events[code].callbacks = (pfn_event_callback *) darray_create(sizeof(registered_event_t));
    }

    darray_push(registered_events[code].callbacks, callback);
}

void event_system_unregister(Event_Code code, pfn_event_callback callback)
{
    ASSERT(code > EVENT_CODE_NONE && code < NUM_OF_EVENT_CODES);
    ASSERT(callback);

    u64 callbacks_length = darray_length(registered_events[code].callbacks);
    for (u64 i = 0; i < callbacks_length; i++) {
        if (registered_events[code].callbacks[i] == callback) {
            darray_pop_at(registered_events[code].callbacks, i, 0);
            return;
        }
    }

    LOG_WARN("failed to find callback to unregister: code %d, callback %p\n", code, callback);
}

void event_system_fire(Event_Code code, Event_Data data)
{
    ASSERT(code > EVENT_CODE_NONE && code < NUM_OF_EVENT_CODES);

    if (registered_events[code].callbacks == 0) {
        LOG_WARN("tried to fire event with no registered callbacks\n");
        return;
    }

    u64 callbacks_length = darray_length(registered_events[code].callbacks);
    for (u64 i = 0; i < callbacks_length; i++) {
        pfn_event_callback callback = registered_events[code].callbacks[i];
        if (callback(code, data)) {
            break;
        }
    }
}
