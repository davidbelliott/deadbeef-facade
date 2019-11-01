#ifndef LIST_H
#define LIST_H

typedef struct list_t {
    void *data;
    struct list_t *prev;
    struct list_t *next;
} list_t;

void insert_before(list_t **head, list_t *pos, void *data);
void insert_after(list_t **head, list_t *pos, void *data);
void push_front(list_t **head, void *data);
void *pop_front(list_t **head);

#endif
