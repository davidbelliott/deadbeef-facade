#include "astar.h"
#include "common.h"

#include <stdlib.h>
#include <string.h>

astar_node_t* astar_alloc_node(int x, int y, astar_node_t *from) {
    astar_node_t *ptr = (astar_node_t*)malloc(sizeof(astar_node_t));
    ptr->pt.x = x;
    ptr->pt.y = y;
    ptr->f = 0;
    ptr->g = 0;
    ptr->h = 0;
    ptr->from = from;
    return ptr;
}

astar_node_t* astar_alloc_copy(astar_node_t *node) {
    astar_node_t *ptr = (astar_node_t*)malloc(sizeof(astar_node_t));
    memcpy(ptr, node, sizeof(astar_node_t));
    return ptr;
}

void astar_insert_sort_f(list_t **head, astar_node_t *node) {
    list_t *ibef = *head;
    if(!ibef) { //empty list: create a new linked list
        push_front(head, node);
    } else {
        bool inserted = false;
        for(; ibef->next; ibef=ibef->next) {
            if(((astar_node_t*)ibef->data)->f > node->f) {
                insert_before(head, ibef, node);
                inserted = true;
                break;
            }
        }
        if(!inserted)
            insert_after(head, ibef, (void*)node);
    }
}

bool astar_in(list_t **head, astar_node_t *node) {
    bool in = false;
    list_t *q = *head;
    while(q) {
        if(whitgl_ivec_eq(((astar_node_t*)q->data)->pt, node->pt)) {
            return true;
        }
        q = q->next;
    }
    return false;
}

list_t* reconstruct_path(astar_node_t *q) {
    list_t *path = NULL;
    while(q) {
        push_front(&path, astar_alloc_copy(q));
        q = q->from;
    }
    return path;
}

void astar_free_node_list(list_t **q) {
    while (*q) {
        astar_node_t *data = (astar_node_t*)pop_front(q);
        free(data);
    }
}

float idiag_distance(whitgl_ivec start, whitgl_ivec end) {
    int max = MAX(abs(start.x - end.x), abs(start.y - end.y));
    int min = MIN(abs(start.x - end.x), abs(start.y - end.y));
    return (float)(max - min) + SQRT2 * (float)min;
}

list_t* astar(whitgl_ivec start, whitgl_ivec end, map_t *map) {
    static const int offsets[] = {-1, 0, 1};
    list_t *open = NULL;
    list_t *closed = NULL;
    push_front(&open, astar_alloc_node(start.x, start.y, NULL));

    for (int n_iters = 0; n_iters < 32 && open; n_iters++) {
        astar_node_t *q = (astar_node_t*)pop_front(&open);
        astar_insert_sort_f(&closed, q);
        list_t *s = NULL;
        int x = q->pt.x;
        int y = q->pt.y;
        if(whitgl_ivec_eq(q->pt, end)) {  // goal reached
            list_t *path = reconstruct_path(q);
            astar_free_node_list(&open);
            astar_free_node_list(&closed);
            return path;
        }
        for(int i = 0; i < 3; i++) {
            int dx = offsets[i];
            for(int j = 0; j < 3; j++) {
                int dy = offsets[j];
                if(     (dx != 0 || dy != 0) // not both zero
                        && x+dx < MAP_WIDTH && x+dx >= 0 && y+dy < MAP_HEIGHT && y+dy >= 0 // within bounds
                        && MAP_GET(map, x+dx, y+dy) == 0 // no wall in target spot
                        && (dx == 0 || dy == 0 || (MAP_GET(map, x, y+dy) == 0 && MAP_GET(map, x+dx, y) == 0))) // valid if diagonal
                {
                    /*if(q->pt.x == 9 && q->pt.y == 5)
                        printf("Found neighbor: (%d, %d)\n", x+dx, y+dy);*/
                    push_front(&s, astar_alloc_node(x+dx, y+dy, q));
                }
            }
        }
        while(s) {
            astar_node_t *s_node = (astar_node_t*)s->data;
            s_node->g = q->g + idiag_distance(s_node->pt, q->pt);
            s_node->h = idiag_distance(s_node->pt, end);
            s_node->f = s_node->g + s_node->h;
            bool add = true;
            add = !astar_in(&closed, s_node);
            list_t *check = open;
            while(add && check) {
                astar_node_t *c_node = (astar_node_t*)check->data;
                if(whitgl_ivec_eq(s_node->pt, c_node->pt) && s_node->g >= c_node->g) {
                    add = false;
                    break;
                }
                check = check->next;
            }
            if(add) {
                astar_insert_sort_f(&open, astar_alloc_copy(s_node));
            }
            s = s->next;
        }
        astar_free_node_list(&s);
    }
    //pathfinding failed
    astar_free_node_list(&open);
    astar_free_node_list(&closed);
    return NULL;
}
