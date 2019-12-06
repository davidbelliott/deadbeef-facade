#ifndef ANIM_H
#define ANIM_H

#include "common.h"
#include <whitgl/sys.h>
#include <whitgl/math.h>

typedef struct anim_obj {
    unsigned int id;
    whitgl_sprite sprite;
    whitgl_ivec frametr;
    int n_frames[MAX_N_ANIM_STATES];
    int delay;  // delay between frames in number of beats (min 1)
    bool loop;
} anim_obj;

anim_obj *anim_create(whitgl_sprite sprite, whitgl_ivec frametr, int n_frames[MAX_N_ANIM_STATES], int delay, bool loop);
void anim_destroy(anim_obj *anim);
void anim_objs_update();

#endif // ANIM_H
