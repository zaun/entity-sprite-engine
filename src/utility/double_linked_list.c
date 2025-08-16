#include <stdlib.h>
#include "utility/double_linked_list.h"
#include "utility/log.h"
#include "core/memory_manager.h"

typedef struct EseDListNode {
    void* value;
    struct EseDListNode* prev;
    struct EseDListNode* next;
} EseDListNode;

struct EseDoubleLinkedList {
    EseDListNode* head;
    EseDListNode* tail;
    size_t size;
    DListFreeFn free_fn;
};

struct EseDListIter {
    EseDListNode* current;
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
