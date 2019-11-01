#include "list.h"
#include <stdlib.h>


list_t *_alloc_node(void *data) {
    list_t *ptr = (list_t*)malloc(sizeof(list_t));
    ptr->data = data;
    ptr->prev = NULL;
    ptr->next = NULL;
    return ptr;
}

void _destr_node(list_t *node) {
    free(node);
}

void insert_before(list_t **head, list_t *pos, void *data) {
    list_t *node = _alloc_node(data);
    if(!pos->prev) { // node is new head of existing list
        pos->prev = node;
        node->prev = NULL;
        node->next = pos;
        *head = node;
    } else {
        pos->prev->next = node;
        node->prev = pos->prev;
        node->next = pos;
        pos->prev = node;
    }
}

void insert_after(list_t **head, list_t *pos, void *data) {
    list_t *node = _alloc_node(data);
    if(!pos->next) {
        pos->next = node;
        node->prev = pos;
        node->next = NULL;
    } else {
        pos->next->prev = node;
        node->next = pos->next;
        node->prev = pos;
        pos->next = node;
    }
}

void push_front(list_t **head, void *data) {
    list_t *node = _alloc_node(data);
    node->next = *head;
    node->prev = NULL;
    if(*head)
        (*head)->prev = node;
    *head = node;
}

void *pop_front(list_t **head) {
    list_t *ptr = *head;
    if(ptr) {
        *head = ptr->next;
        if(*head)
            (*head)->prev = NULL;
    }
    void *data = ptr->data;
    _destr_node(ptr);
    return data;
}
