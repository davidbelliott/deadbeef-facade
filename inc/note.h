#ifndef NOTE_H
#define NOTE_H

#include "anim.h"

#include <stdbool.h>
#include <whitgl/math.h>

typedef struct note_t {
    bool exists;
    char type;
    bool popped;
    bool hold;
    whitgl_ivec pos;
    int channel;
    anim_obj *anim;
} note_t;

void note_create(note_t *note, int exists);
void draw_notes(note_t *beat, int cur_note, float time_since_note, int x);

#endif // NOTE_H
