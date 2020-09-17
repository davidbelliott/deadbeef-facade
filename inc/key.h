#ifndef KEY_H
#define KEY_H

#define KEY_R 0x1
#define KEY_G 0x2
#define KEY_B 0x4

typedef struct key_entity_t {
    whitgl_ivec pos;
    unsigned int type;
} key_entity_t;

int key_create(whitgl_ivec pos, int type, map_t *map);
void key_destroy(int key_idx);
void destroy_all_keys();
int key_at(whitgl_ivec pos);
void draw_keys(whitgl_fmat view, whitgl_fmat persp);

#endif
