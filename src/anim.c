#include "anim.h"
#include <stdlib.h>
#include <string.h>

anim_obj *anim_objs[MAX_N_ANIM_OBJS] = {0};

anim_obj *anim_create(whitgl_sprite sprite, whitgl_ivec frametr, int n_frames[MAX_N_ANIM_STATES], int delay, bool loop) {
    int i = 0;
    for (i = 0; i < MAX_N_ANIM_OBJS; i++) {
        if (!anim_objs[i])
            break;
    }
    anim_obj *anim = NULL;
    if (i < MAX_N_ANIM_OBJS) {
        anim = (anim_obj*)malloc(sizeof(anim_obj));
        anim->id = i;
        anim->sprite = sprite;
        anim->frametr = frametr;
        memcpy(anim->n_frames, n_frames, MAX_N_ANIM_STATES * sizeof(int));
        anim->delay = MAX(1, delay);
        anim->loop = loop;
        anim_objs[i] = anim;
    }
    return anim;
}

void anim_destroy(anim_obj *anim) {
    anim_objs[anim->id] = NULL;
    free(anim);
}

void anim_objs_update() {
    for (int i = 0; i < MAX_N_ANIM_OBJS; i++) {
        if (anim_objs[i]) {
            int n_frames = anim_objs[i]->n_frames[anim_objs[i]->frametr.y];
            int cur_frame = anim_objs[i]->frametr.x;
            if (n_frames > 1) {
                    cur_frame = anim_objs[i]->loop ? (cur_frame + 1) % n_frames : MIN(cur_frame + 1, n_frames - 1);
            }
            anim_objs[i]->frametr.x = cur_frame;
        }
    }
}

