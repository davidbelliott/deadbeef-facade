#include "graphics.h"
#include "key.h"

#include <stdlib.h>

#define MAX_N_KEYS 32

key_entity_t *keys[MAX_N_KEYS] = {0};

int key_create(whitgl_ivec pos, int type, map_t *map) {
    int i;
    for (i = 0; i < MAX_N_KEYS; i++) {
        if (!keys[i]) {
            key_entity_t *key = (key_entity_t*)malloc(sizeof(key));
            key->pos = pos;
            key->type = type;
            //MAP_SET_ENTITY(map, pos.x, pos.y, ENTITY_TYPE_KEY);
            keys[i] = key;
            break;
        }
    }
    return (i != MAX_N_KEYS ? i : -1);
}

void key_destroy(int key_idx) {
    if (0 <= key_idx && key_idx < MAX_N_KEYS) {
        free(keys[key_idx]);
        keys[key_idx] = NULL;
    }
}

void destroy_all_keys() {
    for (int i = 0; i < MAX_N_KEYS; i++) {
        key_destroy(i);
    }
}

static void draw_key(key_entity_t *key, whitgl_fmat view, whitgl_fmat persp) {
    int x_offs;
    if (key->type == KEY_R) {
        x_offs = 0;
    } else if (key->type == KEY_G) {
        x_offs = 64;
    } else if (key->type == KEY_B) {
        x_offs = 128;
    }
    whitgl_fvec offset = {384 + x_offs, 64};
    whitgl_fvec key_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 2, offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 3, key_size);
    whitgl_fvec3 pos = {(float)key->pos.x + 0.5f, (float)key->pos.y + 0.5f, 0.5f};
    draw_billboard(pos, view, persp);
}

void draw_keys(whitgl_fmat view, whitgl_fmat persp) {
    for (int i = 0; i < MAX_N_KEYS; i++) {
        if (keys[i]) {
            draw_key(keys[i], view, persp);
        }
    }
}

int key_at(whitgl_ivec pos) {
    for (int i = 0; i < MAX_N_KEYS; i++) {
        if (keys[i] && whitgl_ivec_eq(keys[i]->pos, pos)) {
            return i;
        }
    }
    return -1;
}
