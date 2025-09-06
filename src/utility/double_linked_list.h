#ifndef ESE_DOUBLE_LINKED_LIST_H
#define ESE_DOUBLE_LINKED_LIST_H

#include <stddef.h>

// Forward declarations
typedef struct EseDoubleLinkedList EseDoubleLinkedList;
typedef struct EseDListIter EseDListIter;

// Predicate: returns nonzero if match, zero otherwise
typedef int (*DListPredicate)(void *value, void *user_data);

// Value free function
typedef void (*DListFreeFn)(void *value);
typedef void* (*DListCopyFn)(void *value);

// Create a new list
EseDoubleLinkedList* dlist_create(DListFreeFn free_fn);

// Create a deep copy of the list
EseDoubleLinkedList* dlist_copy(EseDoubleLinkedList* list, DListCopyFn copy_fn);

// Free the list and all nodes (calls free_fn for each value)
void dlist_free(EseDoubleLinkedList* list);

// Add value to end
void dlist_append(EseDoubleLinkedList* list, void* value);

// Remove a node (by value)
void dlist_remove_by_value(EseDoubleLinkedList* list, void* value);

// Find first node matching predicate
void* dlist_find(EseDoubleLinkedList* list, DListPredicate pred, void *user_data);

// Remove all nodes matching predicate
void dlist_remove_by(EseDoubleLinkedList* list, DListPredicate pred, void *user_data);

// Iterator
EseDListIter* dlist_iter_create(EseDoubleLinkedList* list);
EseDListIter* dlist_iter_create_from(EseDListIter* iter);
void dlist_iter_free(EseDListIter* iter);
int dlist_iter_next(EseDListIter* iter, void** value);

// Get list size
size_t dlist_size(EseDoubleLinkedList* list);

// Pop front node
void* dlist_pop_front(EseDoubleLinkedList* list);

// Clear list
void dlist_clear(EseDoubleLinkedList* list);

// Remove all nodes
void dlist_remove_all(EseDoubleLinkedList* list);

// Remove all nodes matching predicate
void dlist_remove_all_by(EseDoubleLinkedList* list, DListPredicate pred, void *user_data);

#endif // ESE_DOUBLE_LINKED_LIST_H
