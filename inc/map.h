#ifndef MAP_H
#define MAP_H

typedef struct map_t {
    unsigned char *tiles;
    unsigned char *entities;
    int w;
    int h;
} map_t;

#define MAP_TILE(map, x, y) (map)->tiles[(y) * (map)->h + (x)]
#define MAP_ENTITY(map, x, y) (map)->entities[(y) * (map)->h + (x)]
#define MAP_SET_ENTITY(map, x, y, ent) ((map)->entities[(y) * (map)->h + (x)] = ent)

#endif // MAP_H
