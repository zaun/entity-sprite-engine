#ifndef ESE_pubsub_H
#define ESE_pubsub_H

#include "scripting/lua_value.h"

// Forward declarations
typedef struct EsePubSub EsePubSub;
typedef struct EseEntity EseEntity;

/**
 * @brief Creates a new pub/sub system instance.
 *
 * @return A pointer to the newly created EsePubSub instance.
 */
EsePubSub *ese_pubsub_create(void);

/**
 * @brief Destroys a pub/sub system instance and frees its resources.
 *
 * @param pub_sub A pointer to the EsePubSub instance to destroy.
 */
void ese_pubsub_destroy(EsePubSub *pub_sub);

/**
 * @brief Publishes data to a topic.
 *
 * @param pub_sub A pointer to the EsePubSub instance.
 * @param name The topic name to publish to.
 * @param data The EseLuaValue data to publish.
 */
void ese_pubsub_pub(EsePubSub *pub_sub, const char *name,
                    const EseLuaValue *data);

/**
 * @brief Subscribes an entity to a topic with a function name.
 *
 * @param pub_sub A pointer to the EsePubSub instance.
 * @param name The topic name to subscribe to.
 * @param entity The entity to call the function on.
 * @param function_name The name of the function to call on the entity.
 */
void ese_pubsub_sub(EsePubSub *pub_sub, const char *name, EseEntity *entity,
                    const char *function_name);

/**
 * @brief Unsubscribes an entity from a topic.
 *
 * @param pub_sub A pointer to the EsePubSub instance.
 * @param name The topic name to unsubscribe from.
 * @param entity The entity to unsubscribe.
 * @param function_name The name of the function that was subscribed.
 */
void ese_pubsub_unsub(EsePubSub *pub_sub, const char *name, EseEntity *entity,
                      const char *function_name);

#endif // ESE_pubsub_H
