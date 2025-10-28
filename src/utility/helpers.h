/* helpers.h
 *
 * Not suitable for cryptographic use.
 *
 */

#ifndef ESE_HELPERS_H
#define ESE_HELPERS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes a fast 64-bit hash for a memory buffer.
 *
 * @note Not suitable for cryptographic purposes.
 *
 * @param data Pointer to the start of the buffer. Must be non-NULL if size > 0.
 * @param size Number of bytes to hash from the buffer.
 * @return 64-bit hash value for the provided data.
 */
uint64_t ese_helper_hash(void *data, size_t size);

/**
 * @brief Splits a string by a colon into a group and a name.
 *
 * @note `gbuf` and `nbuf` are guaranteed to be null terminated.
 * @note `gbuf` defaults to "default" if no group is provided or input is
 * invalid.
 * @note `nbuf` defaults to "" if no name is provided or input is invalid.
 *
 * @param in The constant string to split.
 * @param gbuf A pointer to a char* where the group string will be stored. Must
 * be at least gsz bytes long.
 * @param gsz The size of the group buffer.
 * @param nbuf A pointer to a char* where the name string will be stored. Must
 * be at least nsz bytes long.
 * @param nsz The size of the name buffer.
 */
void ese_helper_split(const char *in, char *gbuf, size_t gsz, char *nbuf,
                      size_t nsz);

#ifdef __cplusplus
}
#endif

#endif /* ESE_HELPERS_H */
