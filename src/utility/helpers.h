/* helpers.h
 *
 * Not suitable for cryptographic use.
 *
 */

 #ifndef ESE_HELPERS_H
 #define ESE_HELPERS_H
 
 #include <stdint.h>
 #include <stddef.h>
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
 * @note `out_group` and `out_name` caller is responsible for freeing this memory.
 * @note `out_group` and `out_name` are never NULL;
 *       defaults to "default" and "" if no group or name is provided or input is invalid.
 *
 * @param input The constant string to split.
 * @param out_group A pointer to a char* where the group string will be stored.
 * @param out_name A pointer to a char* where the name string will be stored.
 */
 void ese_helper_split(const char *input, char **out_group, char **out_name);

 #ifdef __cplusplus
 }
 #endif
 
 #endif /* ESE_HELPERS_H */
