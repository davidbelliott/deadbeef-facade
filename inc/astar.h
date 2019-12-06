#ifndef ASTAR_H
#define ASTAR_H

#include "list.h"
#include "map.h"

#include <whitgl/math.h>

typedef struct astar_node_t {
    whitgl_ivec pt;
    double f, g, h;
    struct astar_node_t *from;
} astar_node_t;


void astar_free_node_list(list_t **q);
list_t* astar(whitgl_ivec start, whitgl_ivec end, map_t *map);




#endif // ASTAR_H
