#include "pubsub.h"
#include "entity/entity.h"
#include "entity/entity_private.h"
#include "memory_manager.h"
#include "utility/array.h"
#include "utility/hashmap.h"
#include "utility/log.h"
#include "utility/thread.h"
#include <string.h>

/**
 * @brief Internal structure representing a single subscription.
 */
typedef struct {
    EseEntity *entity;   // For entity-based subscriptions
    char *function_name; // For entity function calls
} EseSubscription;

/**
 * @brief Internal pub/sub structure.
 */
struct EsePubSub {
    EseHashMap *topics; // Maps topic names to arrays of subscriptions
    EseMutex *mutex;    // Mutex for thread-safe hashmap operations
};

// Forward declarations for private functions
static void _ese_subscription_free(void *value);
static EseArray *_ese_get_or_create_topic_subscriptions(EsePubSub *pub_sub, const char *name);
static void _entity_pubsub_callback(const char *name, const EseLuaValue *data,
                                    EseSubscription *subscription);

EsePubSub *ese_pubsub_create(void) {
    EsePubSub *pub_sub = memory_manager.malloc(sizeof(EsePubSub), MMTAG_PUB_SUB);
    log_assert("pub_sub", pub_sub != NULL, "Failed to allocate pub_sub");

    pub_sub->topics = hashmap_create(_ese_subscription_free);
    log_assert("pub_sub", pub_sub->topics != NULL, "Failed to create topics hashmap");

    pub_sub->mutex = ese_mutex_create();
    log_assert("pub_sub", pub_sub->mutex != NULL, "Failed to create pubsub mutex");

    return pub_sub;
}

void ese_pubsub_destroy(EsePubSub *pub_sub) {
    if (pub_sub == NULL) {
        return;
    }

    hashmap_destroy(pub_sub->topics);
    if (pub_sub->mutex) {
        ese_mutex_destroy(pub_sub->mutex);
    }
    memory_manager.free(pub_sub);
}

void ese_pubsub_pub(EsePubSub *pub_sub, const char *name, const EseLuaValue *data) {
    log_assert("pub_sub", pub_sub != NULL, "pub_sub cannot be NULL");
    log_assert("pub_sub", name != NULL, "topic name cannot be NULL");
    log_assert("pub_sub", data != NULL, "data cannot be NULL");

    ese_mutex_lock(pub_sub->mutex);
    EseArray *subscriptions = hashmap_get(pub_sub->topics, name);
    if (subscriptions == NULL) {
        ese_mutex_unlock(pub_sub->mutex);
        return;
    }

    // Copy subscriptions to process outside the lock to avoid deadlock
    // if callbacks try to modify pubsub
    size_t count = array_size(subscriptions);
    EseSubscription **subscription_copy = NULL;
    if (count > 0) {
        subscription_copy = memory_manager.malloc(sizeof(EseSubscription *) * count, MMTAG_PUB_SUB);
        for (size_t i = 0; i < count; i++) {
            subscription_copy[i] = array_get(subscriptions, i);
        }
    }
    ese_mutex_unlock(pub_sub->mutex);

    // Process callbacks outside the lock
    if (subscription_copy) {
        for (size_t i = 0; i < count; i++) {
            if (subscription_copy[i] != NULL) {
                _entity_pubsub_callback(name, data, subscription_copy[i]);
            }
        }
        memory_manager.free(subscription_copy);
    }
}

// Private function implementations

static void _ese_subscription_free(void *value) {
    if (value == NULL) {
        return;
    }

    EseArray *subscriptions = (EseArray *)value;
    size_t count = array_size(subscriptions);

    for (size_t i = 0; i < count; i++) {
        EseSubscription *subscription = array_get(subscriptions, i);
        if (subscription != NULL) {
            if (subscription->function_name) {
                memory_manager.free(subscription->function_name);
            }
            memory_manager.free(subscription);
        }
    }

    array_destroy(subscriptions);
}

static EseArray *_ese_get_or_create_topic_subscriptions(EsePubSub *pub_sub, const char *name) {
    ese_mutex_lock(pub_sub->mutex);
    EseArray *subscriptions = hashmap_get(pub_sub->topics, name);

    if (subscriptions == NULL) {
        subscriptions = array_create(4, NULL);
        log_assert("pub_sub", subscriptions != NULL, "Failed to create subscriptions array");
        // hashmap_set copies the key internally, no need to strdup here
        hashmap_set(pub_sub->topics, name, subscriptions);
    }

    ese_mutex_unlock(pub_sub->mutex);
    return subscriptions;
}

void ese_pubsub_sub(EsePubSub *pub_sub, const char *name, EseEntity *entity,
                    const char *function_name) {
    log_assert("pub_sub", pub_sub != NULL, "pub_sub cannot be NULL");
    log_assert("pub_sub", name != NULL, "topic name cannot be NULL");
    log_assert("pub_sub", entity != NULL, "entity cannot be NULL");
    log_assert("pub_sub", function_name != NULL, "function_name cannot be NULL");

    EseArray *subscriptions = _ese_get_or_create_topic_subscriptions(pub_sub, name);

    EseSubscription *subscription = memory_manager.malloc(sizeof(EseSubscription), MMTAG_PUB_SUB);
    log_assert("pub_sub", subscription != NULL, "Failed to allocate subscription");

    subscription->entity = entity;
    subscription->function_name = memory_manager.strdup(function_name, MMTAG_PUB_SUB);

    ese_mutex_lock(pub_sub->mutex);
    array_push(subscriptions, subscription);
    ese_mutex_unlock(pub_sub->mutex);
}

void ese_pubsub_unsub(EsePubSub *pub_sub, const char *name, EseEntity *entity,
                      const char *function_name) {
    log_assert("pub_sub", pub_sub != NULL, "pub_sub cannot be NULL");
    log_assert("pub_sub", name != NULL, "topic name cannot be NULL");
    log_assert("pub_sub", entity != NULL, "entity cannot be NULL");
    log_assert("pub_sub", function_name != NULL, "function_name cannot be NULL");

    ese_mutex_lock(pub_sub->mutex);
    EseArray *subscriptions = hashmap_get(pub_sub->topics, name);
    if (subscriptions == NULL) {
        ese_mutex_unlock(pub_sub->mutex);
        return;
    }

    size_t count = array_size(subscriptions);
    EseSubscription *found_subscription = NULL;
    size_t found_index = 0;
    bool found = false;

    for (size_t i = 0; i < count; i++) {
        EseSubscription *subscription = array_get(subscriptions, i);
        if (subscription != NULL && subscription->entity == entity &&
            strcmp(subscription->function_name, function_name) == 0) {
            found_subscription = subscription;
            found_index = i;
            found = true;
            break;
        }
    }

    if (found) {
        // Remove from array; array_remove_at will free the item if a
        // free_fn was set. Since the array was created with free_fn = NULL,
        // free explicitly here.
        array_remove_at(subscriptions, found_index);
    }

    bool should_remove_topic = (array_size(subscriptions) == 0);
    ese_mutex_unlock(pub_sub->mutex);

    // Free subscription outside the lock
    if (found && found_subscription) {
        if (found_subscription->function_name) {
            memory_manager.free(found_subscription->function_name);
        }
        memory_manager.free(found_subscription);
    }

    // If no more subscriptions, remove the topic
    if (should_remove_topic) {
        ese_mutex_lock(pub_sub->mutex);
        // hashmap_remove returns the stored value without freeing it; free
        // explicitly
        EseArray *removed = (EseArray *)hashmap_remove(pub_sub->topics, name);
        ese_mutex_unlock(pub_sub->mutex);
        if (removed) {
            _ese_subscription_free(removed);
        }
    }
}

// Entity callback function
static void _entity_pubsub_callback(const char *name, const EseLuaValue *data,
                                    EseSubscription *subscription) {
    EseEntity *entity = subscription->entity;
    const char *function_name = subscription->function_name;

    // Check if entity is still valid
    if (entity->destroyed || !entity->active) {
        return;
    }

    // Create arguments: event_name and data
    EseLuaValue *event_name = lua_value_create_string("event_name", name);
    EseLuaValue *args[] = {event_name, (EseLuaValue *)data};

    // Call entity function with the correct function name
    entity_run_function_with_args(entity, function_name, 2, args);

    lua_value_destroy(event_name);
}