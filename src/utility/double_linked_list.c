#include <stdlib.h>
#include "utility/double_linked_list.h"
#include "utility/log.h"
#include "core/memory_manager.h"

/**
 * @brief Node structure for doubly-linked list entries.
 * 
 * @details Each node contains a value pointer and pointers to the previous
 *          and next nodes in the list, enabling bidirectional traversal.
 */
typedef struct EseDListNode {
    void* value;                    /** Pointer to the stored value */
    struct EseDListNode* prev;      /** Pointer to the previous node */
    struct EseDListNode* next;      /** Pointer to the next node */
} EseDListNode;

/**
 * @brief Doubly-linked list data structure.
 * 
 * @details This structure implements a doubly-linked list with head and tail
 *          pointers for efficient insertion and removal at both ends. It also
 *          tracks the list size and provides optional cleanup functions for
 *          stored values.
 */
struct EseDoubleLinkedList {
    EseDListNode* head;             /** Pointer to the first node in the list */
    EseDListNode* tail;             /** Pointer to the last node in the list */
    size_t size;                    /** Number of nodes in the list */
    DListFreeFn free_fn;            /** Optional function to free stored values */
};

/**
 * @brief Iterator structure for traversing doubly-linked list entries.
 * 
 * @details This structure maintains the current position in the list
 *          for iteration, allowing sequential access to list elements.
 */
struct EseDListIter {
    EseDListNode* current;          /** Pointer to the current node being iterated */
};

void _dlist_remove(EseDoubleLinkedList* list, EseDListNode* node) {
    log_assert("DLIST", list, "_dlist_remove called with NULL list");
    log_assert("DLIST", node, "_dlist_remove called with NULL node");

    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->prev = node->prev;
    else list->tail = node->prev;
    if (list->free_fn) list->free_fn(node->value);
    memory_manager.free(node);
    list->size--;
}

EseDoubleLinkedList* dlist_create(DListFreeFn free_fn) {
    EseDoubleLinkedList* list = memory_manager.malloc(sizeof(EseDoubleLinkedList), MMTAG_LINKED_LIST);
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->free_fn = free_fn;
    log_verbose("DLIST", "create %p", (void*)list);
    return list;
}

EseDoubleLinkedList* dlist_copy(EseDoubleLinkedList* list, DListCopyFn copy_fn) {
    log_assert("DLIST", list, "dlist_copy called with NULL list");
    log_assert("DLIST", copy_fn, "dlist_copy called with NULL copy_fn");

    EseDoubleLinkedList* new_list = dlist_create(list->free_fn);
    EseDListNode* node = list->head;
    while (node) {
        void* new_value = copy_fn(node->value);
        if (new_value) {
            dlist_append(new_list, new_value);
        }
        node = node->next;
    }

    return new_list;
}

void dlist_free(EseDoubleLinkedList* list) {
    log_assert("DLIST", list, "dlist_free called with NULL list");

    EseDListNode* node = list->head;
    while (node) {
        EseDListNode* next = node->next;
        if (list->free_fn) list->free_fn(node->value);
        memory_manager.free(node);
        node = next;
    }
    log_verbose("DLIST", "free %p", (void*)list);
    memory_manager.free(list);
}

void dlist_append(EseDoubleLinkedList* list, void* value) {
    log_assert("DLIST", list, "dlist_append called with NULL list");
    log_assert("DLIST", value, "dlist_append called with NULL value");

    EseDListNode* node = memory_manager.malloc(sizeof(EseDListNode), MMTAG_LINKED_LIST);
    node->value = value;
    node->prev = list->tail;
    node->next = NULL;
    if (list->tail) list->tail->next = node;
    else list->head = node;
    list->tail = node;
    list->size++;
}

void dlist_remove_by_value(EseDoubleLinkedList* list, void* value) {
    log_assert("DLIST", list, "dlist_remove_by_value called with NULL list");
    log_assert("DLIST", value, "dlist_remove_by_value called with NULL value");

    EseDListNode* node = list->head;
    while (node) {
        if (node->value == value) {
            _dlist_remove(list, node);
            return; // Remove only the first match
        }
        node = node->next;
    }
}

void* dlist_find(EseDoubleLinkedList* list, DListPredicate pred, void *user_data) {
    log_assert("DLIST", list, "dlist_find called with NULL list");
    log_assert("DLIST", pred, "dlist_find called with NULL pred");

    EseDListNode* node = list->head;
    while (node) {
        if (pred(node->value, user_data)) return node->value;
        node = node->next;
    }
    return NULL;
}

void dlist_remove_by(EseDoubleLinkedList* list, DListPredicate pred, void *user_data) {
    log_assert("DLIST", list, "dlist_remove_by called with NULL list");
    log_assert("DLIST", pred, "dlist_remove_by called with NULL pred");
    
    EseDListNode* node = list->head;
    while (node) {
        EseDListNode* next = node->next;
        if (pred(node->value, user_data)) _dlist_remove(list, node);
        node = next;
    }
}

EseDListIter* dlist_iter_create(EseDoubleLinkedList* list) {
    log_assert("DLIST", list, "dlist_iter_create called with NULL list");

    EseDListIter* iter = memory_manager.malloc(sizeof(EseDListIter), MMTAG_LINKED_LIST);
    iter->current = list->head;
    return iter;
}

EseDListIter* dlist_iter_create_from(EseDListIter* iter) {
    log_assert("DLIST", iter, "dlist_iter_create_from called with NULL iter");

    EseDListIter* new_iter = memory_manager.malloc(sizeof(EseDListIter), MMTAG_LINKED_LIST);
    if (new_iter) {
        // The new iterator just needs to point to the same current node
        new_iter->current = iter->current;
    }
    return new_iter;
}

void dlist_iter_free(EseDListIter* iter) {
    log_assert("DLIST", iter, "dlist_iter_free called with NULL iter");

    memory_manager.free(iter);
}

int dlist_iter_next(EseDListIter* iter, void** value) {
    log_assert("DLIST", iter, "dlist_iter_next called with NULL iter");
    log_assert("DLIST", value, "dlist_iter_next called with NULL value");
    
    if (!iter->current) return 0;
    *value = iter->current->value;
    iter->current = iter->current->next;
    return 1;
}

size_t dlist_size(EseDoubleLinkedList* list) {
    log_assert("DLIST", list, "dlist_size called with NULL list");

    return list->size;
}

void* dlist_pop_front(EseDoubleLinkedList* list) {
    log_assert("DLIST", list, "dlist_pop_front called with NULL list");
    if (!list->head) return NULL;
    EseDListNode *node = list->head;
    void *val = node->value;
    if (node->next) {
        node->next->prev = NULL;
        list->head = node->next;
    } else {
        list->head = list->tail = NULL;
    }
    memory_manager.free(node);
    list->size--;
    return val;
}

void* dlist_pop_back(EseDoubleLinkedList* list) {
    log_assert("DLIST", list, "dlist_pop_back called with NULL list");
    if (!list->tail) return NULL;
    EseDListNode *node = list->tail;
    void *val = node->value;
    if (node->prev) {
        node->prev->next = NULL;
        list->tail = node->prev;
    } else {
        list->head = list->tail = NULL;
    }
    memory_manager.free(node);
    list->size--;
    return val;
}

void dlist_clear(EseDoubleLinkedList* list) {
    log_assert("DLIST", list, "dlist_clear called with NULL list");
    void *v;
    while ((v = dlist_pop_front(list)) != NULL) {
        if (list->free_fn) list->free_fn(v);
    }
}
