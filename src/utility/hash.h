/* fast_hast.h
 *
 * Not suitable for cryptographic use.
 *
 */

 #ifndef ESE_HASH_H
 #define ESE_HASH_H
 
 #include <stdint.h>
 #include <stddef.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 uint64_t ese_hash(void *data, size_t size);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* ESE_HASH_H */
