#include "profile.h"

#ifdef ESE_PROFILE_ENABLED

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "platform/time.h"

// --- Hash table for profile stats ---
typedef struct {
    char *key;
    uint64_t total;
    uint64_t count;
    uint64_t max;
    int used;
} ProfileEntry;

#define PROFILE_TABLE_SIZE 1024
static ProfileEntry table[PROFILE_TABLE_SIZE];

static unsigned hash_str(const char *s) {
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)(*s++);
    return h;
}

static ProfileEntry *find_entry(const char *key, int create) {
    unsigned h = hash_str(key);
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        unsigned idx = (h + i) % PROFILE_TABLE_SIZE;
        ProfileEntry *e = &table[idx];
        if (e->used) {
            if (strcmp(e->key, key) == 0) return e;
        } else if (create) {
            e->key = strdup(key);
            e->total = 0;
            e->count = 0;
            e->max = 0;
            e->used = 1;
            return e;
        } else {
            return NULL;
        }
    }
    return NULL; // table full
}

void profile_time(const char *key, uint64_t ns) {
    ProfileEntry *e = find_entry(key, 1);
    e->total += ns;
    e->count++;
    if (ns > e->max) e->max = ns;
}

uint64_t profile_get_max(const char *key) {
    ProfileEntry *e = find_entry(key, 0);
    return e ? e->max : 0;
}

uint64_t profile_get_average(const char *key) {
    ProfileEntry *e = find_entry(key, 0);
    return (e && e->count) ? e->total / e->count : 0;
}

uint64_t profile_get_count(const char *key) {
    ProfileEntry *e = find_entry(key, 0);
    return e ? e->count : 0;
}

void profile_clear(const char *key) {
    ProfileEntry *e = find_entry(key, 0);
    if (e) {
        e->total = 0;
        e->count = 0;
        e->max = 0;
    }
}

void profile_reset_all(void) {
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        if (table[i].used) {
            table[i].total = 0;
            table[i].count = 0;
            table[i].max = 0;
        }
    }
}

// Comparator for qsort (sort by key name)
static int profile_entry_cmp(const void *a, const void *b) {
    const ProfileEntry *ea = *(const ProfileEntry **)a;
    const ProfileEntry *eb = *(const ProfileEntry **)b;
    return strcmp(ea->key, eb->key);
}

// Comparator for string keys (sort by key name)
static int profile_key_cmp(const void *a, const void *b) {
    const char *key_a = *(const char **)a;
    const char *key_b = *(const char **)b;
    return strcmp(key_a, key_b);
}

// --- Count table for simple counting (no timing) ---
typedef struct {
    char *key;
    uint64_t count;
    int used;
} ProfileCountEntry;

#define PROFILE_COUNT_TABLE_SIZE 1024
static ProfileCountEntry count_table[PROFILE_COUNT_TABLE_SIZE];

static ProfileCountEntry *find_count_entry(const char *key, int create) {
    unsigned h = hash_str(key);
    for (unsigned i = 0; i < PROFILE_COUNT_TABLE_SIZE; i++) {
        unsigned idx = (h + i) % PROFILE_COUNT_TABLE_SIZE;
        ProfileCountEntry *e = &count_table[idx];
        if (e->used) {
            if (strcmp(e->key, key) == 0) return e;
        } else if (create) {
            e->key = strdup(key);
            e->count = 0;
            e->used = 1;
            return e;
        } else {
            return NULL;
        }
    }
    return NULL; // table full
}

void profile_count_add(const char *key) {
    ProfileCountEntry *e = find_count_entry(key, 1);
    e->count++;
}

void profile_count_remove(const char *key) {
    ProfileCountEntry *e = find_count_entry(key, 0);
    if (e && e->count > 0) {
        e->count--;
    }
}

uint64_t profile_count_get(const char *key) {
    ProfileCountEntry *e = find_count_entry(key, 0);
    return e ? e->count : 0;
}

void profile_count_clear(const char *key) {
    ProfileCountEntry *e = find_count_entry(key, 0);
    if (e) {
        e->count = 0;
    }
}

void profile_count_reset_all(void) {
    for (unsigned i = 0; i < PROFILE_COUNT_TABLE_SIZE; i++) {
        if (count_table[i].used) {
            count_table[i].count = 0;
        }
    }
}

// Comparator for count entries (sort by key name)
static int profile_count_entry_cmp(const void *a, const void *b) {
    const ProfileCountEntry *ea = *(const ProfileCountEntry **)a;
    const ProfileCountEntry *eb = *(const ProfileCountEntry **)b;
    return strcmp(ea->key, eb->key);
}

// --- Timer support (100 slots) ---
#define PROFILE_MAX_TIMERS 100
static uint64_t timers[PROFILE_MAX_TIMERS];

// --- Snapshot storage ---
typedef struct {
    char *name;
    ProfileSnapshot *entries;
    size_t entry_count;
    int used;
} ProfileSnapshotStorage;

#define PROFILE_MAX_SNAPSHOTS 100
static ProfileSnapshotStorage snapshots[PROFILE_MAX_SNAPSHOTS];
static size_t snapshot_count = 0;

static ProfileSnapshotStorage *find_snapshot_storage(const char *name) {
    for (unsigned i = 0; i < PROFILE_MAX_SNAPSHOTS; i++) {
        if (snapshots[i].used && strcmp(snapshots[i].name, name) == 0) {
            return &snapshots[i];
        }
    }
    return NULL;
}

static ProfileSnapshotStorage *create_snapshot_storage(const char *name) {
    for (unsigned i = 0; i < PROFILE_MAX_SNAPSHOTS; i++) {
        if (!snapshots[i].used) {
            snapshots[i].name = strdup(name);
            snapshots[i].entries = NULL;
            snapshots[i].entry_count = 0;
            snapshots[i].used = 1;
            snapshot_count++;
            return &snapshots[i];
        }
    }
    return NULL; // no free slots
}

void profile_snapshot(const char *name) {
    ProfileSnapshotStorage *storage = find_snapshot_storage(name);
    if (storage) {
        // Clear existing snapshot
        for (size_t i = 0; i < storage->entry_count; i++) {
            free(storage->entries[i].key);
            free(storage->entries[i].snapshot_name);
        }
        free(storage->entries);
        storage->entries = NULL;
        storage->entry_count = 0;
    } else {
        storage = create_snapshot_storage(name);
        if (!storage) return; // no free slots
    }
    
    // Count how many active entries we have
    size_t active_count = 0;
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        if (table[i].used && table[i].count > 0) {
            active_count++;
        }
    }
    
    if (active_count == 0) return;
    
    // Allocate storage for snapshot
    storage->entries = malloc(active_count * sizeof(ProfileSnapshot));
    if (!storage->entries) return;
    
    // Copy active entries to snapshot
    size_t idx = 0;
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        if (table[i].used && table[i].count > 0) {
            storage->entries[idx].key = strdup(table[i].key);
            storage->entries[idx].total = table[i].total;
            storage->entries[idx].count = table[i].count;
            storage->entries[idx].max = table[i].max;
            storage->entries[idx].snapshot_name = strdup(name);
            idx++;
        }
    }
    storage->entry_count = active_count;
    
    // Reset active tables
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        if (table[i].used) {
            table[i].total = 0;
            table[i].count = 0;
            table[i].max = 0;
        }
    }
}

ProfileSnapshot* profile_snapshot_get(const char *name) {
    ProfileSnapshotStorage *storage = find_snapshot_storage(name);
    if (!storage) return NULL;
    return storage->entries;
}

void profile_start(int id) {
    if (id < 0 || id >= PROFILE_MAX_TIMERS) return;
    timers[id] = time_now();
}

void profile_cancel(int id) {
    if (id < 0 || id >= PROFILE_MAX_TIMERS) return;
    timers[id] = 0; // reset
}

void profile_stop(int id, const char *key) {
    if (id < 0 || id >= PROFILE_MAX_TIMERS) return;
    uint64_t end = time_now();
    uint64_t start = timers[id];
    if (start != 0) {
        profile_time(key, end - start);
        timers[id] = 0; // reset
    }
}

void profile_display(void) {
    printf("\n================================ Profile Stats ================================\n");
    printf("%-36s | %10s | %12s | %12s\n",
           "Key", "Count", "Average (ns)", "Max (ns)");
    printf("-------------------------------------+------------+--------------+-------------\n");



    // First, collect all unique keys from current data and snapshots
    char *all_keys[PROFILE_TABLE_SIZE * 2]; // Allow for duplicate keys
    size_t unique_key_count = 0;
    
    // Add ALL current active keys (including those with zero counts)
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        if (table[i].used) {
            all_keys[unique_key_count++] = table[i].key;
        }
    }
    
    // Add snapshot keys (avoid duplicates)
    for (unsigned i = 0; i < PROFILE_MAX_SNAPSHOTS; i++) {
        if (snapshots[i].used) {
            for (size_t j = 0; j < snapshots[i].entry_count; j++) {
                const char *snap_key = snapshots[i].entries[j].key;
                // Check if this key is already in our list
                int found = 0;
                for (size_t k = 0; k < unique_key_count; k++) {
                    if (strcmp(all_keys[k], snap_key) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    all_keys[unique_key_count++] = (char*)snap_key;
                }
            }
        }
    }
    
    // Sort all keys alphabetically
    qsort(all_keys, unique_key_count, sizeof(char*), profile_key_cmp);
    
    // Display each unique key with current data and snapshots
    for (size_t i = 0; i < unique_key_count; i++) {
        const char *key = all_keys[i];
        
        // Find current data for this key
        ProfileEntry *current_entry = NULL;
        for (unsigned j = 0; j < PROFILE_TABLE_SIZE; j++) {
            if (table[j].used && strcmp(table[j].key, key) == 0) {
                current_entry = &table[j];
                break;
            }
        }
        
        // Display current data (if any)
        if (current_entry && current_entry->count > 0) {
            uint64_t avg = current_entry->total / current_entry->count;
            printf("%-36s | %10llu | %12llu | %12llu\n",
                   key,
                   (unsigned long long)current_entry->count,
                   (unsigned long long)avg,
                   (unsigned long long)current_entry->max);
        } else {
            // No current data, just show the key with zeros
            printf("%-36s | %10s | %12s | %12s\n",
                   key, "0", "0", "0");
        }
        
        // Display all snapshots for this key
        for (unsigned j = 0; j < PROFILE_MAX_SNAPSHOTS; j++) {
            if (snapshots[j].used) {
                for (size_t k = 0; k < snapshots[j].entry_count; k++) {
                    if (strcmp(snapshots[j].entries[k].key, key) == 0) {
                        uint64_t snap_avg = snapshots[j].entries[k].total / snapshots[j].entries[k].count;
                        printf("  %-34s | %10llu | %12llu | %12llu\n",
                               snapshots[j].entries[k].snapshot_name,
                               (unsigned long long)snapshots[j].entries[k].count,
                               (unsigned long long)snap_avg,
                               (unsigned long long)snapshots[j].entries[k].max);
                    }
                }
            }
        }
    }

    printf("===============================================================================\n\n");
    
    // Display count table entries
    printf("===================== Profile Counts =====================\n");
    printf("%-44s | %10s\n", "Key", "Count");
    printf("---------------------------------------------+------------\n");

    // Collect count entries
    ProfileCountEntry *count_entries[PROFILE_COUNT_TABLE_SIZE];
    size_t count_n = 0;
    for (unsigned i = 0; i < PROFILE_COUNT_TABLE_SIZE; i++) {
        if (count_table[i].used && count_table[i].count > 0) {
            count_entries[count_n++] = &count_table[i];
        }
    }

    qsort(count_entries, count_n, sizeof(ProfileCountEntry *), profile_count_entry_cmp);

    // Print sorted count entries
    for (size_t i = 0; i < count_n; i++) {
        ProfileCountEntry *e = count_entries[i];
        printf("%-44s | %10llu\n",
               e->key,
               (unsigned long long)e->count);
    }

    printf("==========================================================\n\n");
}

void profile_destroy(void) {
    for (unsigned i = 0; i < PROFILE_TABLE_SIZE; i++) {
        if (table[i].used) {
            free(table[i].key);
            table[i].key = NULL;
            table[i].used = 0;
        }
    }
    
    for (unsigned i = 0; i < PROFILE_COUNT_TABLE_SIZE; i++) {
        if (count_table[i].used) {
            free(count_table[i].key);
            count_table[i].key = NULL;
            count_table[i].used = 0;
        }
    }
    
    // Clean up snapshots
    for (unsigned i = 0; i < PROFILE_MAX_SNAPSHOTS; i++) {
        if (snapshots[i].used) {
            free(snapshots[i].name);
            snapshots[i].name = NULL;
            for (size_t j = 0; j < snapshots[i].entry_count; j++) {
                free(snapshots[i].entries[j].key);
                free(snapshots[i].entries[j].snapshot_name);
            }
            free(snapshots[i].entries);
            snapshots[i].entries = NULL;
            snapshots[i].entry_count = 0;
            snapshots[i].used = 0;
        }
    }
    snapshot_count = 0;
}

#endif // ESE_PROFILE_ENABLED