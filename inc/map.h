#ifndef MAP_H
#define MAP_H

typedef struct map_t {
    unsigned char *data;
    int w;
    int h;
} map_t;

#define MAP_GET(map, x, y) (map)->data[(y) * (map)->h + (x)]

#endif // MAP_H
