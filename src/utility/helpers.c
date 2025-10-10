#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "core/memory_manager.h"

static inline uint64_t _ese_helper_rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

uint64_t ese_helper_hash(void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + size;
    uint64_t h = 0x84222325CBF29CE4ULL; /* seed-ish constant */
    const uint64_t kMul = 0x9E3779B97F4A7C15ULL; /* golden-ish prime */

    while ((end - p) >= 8) {
        uint64_t w;
        memcpy(&w, p, 8); /* safe unaligned load */
        p += 8;
        w *= kMul;
        w = _ese_helper_rotl64(w, 31);
        w *= kMul;
        h ^= w;
        h = _ese_helper_rotl64(h, 27) * kMul + 0x52dce729;
    }

    /* tail */
    uint64_t tail = 0;
    switch (end - p) {
        case 7: tail |= (uint64_t)p[6] << 48;
        case 6: tail |= (uint64_t)p[5] << 40;
        case 5: tail |= (uint64_t)p[4] << 32;
        case 4: tail |= (uint64_t)p[3] << 24;
        case 3: tail |= (uint64_t)p[2] << 16;
        case 2: tail |= (uint64_t)p[1] << 8;
        case 1: tail |= (uint64_t)p[0];
                tail *= kMul;
                tail = _ese_helper_rotl64(tail, 31);
                tail *= kMul;
                h ^= tail;
    }

    /* finalization (fmix-like) */
    h ^= (uint64_t)size;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;

    return h;
}

 void ese_helper_split(const char *input, char **out_group, char **out_name)
 {
     // Initialize outputs to safe defaults: group="default", name=""
     *out_group = memory_manager.strdup("default", MMTAG_GENERAL);
     *out_name = memory_manager.strdup("", MMTAG_GENERAL);
 
     // If input is NULL or empty, keep defaults and return
     if (input == NULL) {
         return;
     }
 
     const char *colon = strchr(input, ':');
     if (colon == NULL) {
         // No colon: entire input is the name
         memory_manager.free(*out_name);
         *out_name = memory_manager.strdup(input, MMTAG_GENERAL);
         return;
     }
 
     size_t groupLength = (size_t)(colon - input);
     const char *nameStart = colon + 1;
     size_t nameLength = strlen(nameStart);
 
     // Set group if provided before colon
     if (groupLength > 0) {
         memory_manager.free(*out_group);
         *out_group = (char *)memory_manager.malloc(groupLength + 1, MMTAG_GENERAL);
         strncpy(*out_group, input, groupLength);
         (*out_group)[groupLength] = '\0';
     }
 
     // Set name (empty string if none provided)
     if (nameLength > 0) {
         memory_manager.free(*out_name);
         *out_name = memory_manager.strdup(nameStart, MMTAG_GENERAL);
     }
 }