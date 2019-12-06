#include "note.h"

#include <whitgl/sys.h>

whitgl_ivec note_pop_pos = {SCREEN_W / 2 - 8, SCREEN_H / 2 - 8};

void note_create(note_t *note, int exists) {
    if (exists) {
        whitgl_sprite note_sprite = {0, {128,64},{16,16}};
        whitgl_ivec frametr = {0, 0};
        int n_frames[MAX_N_ANIM_STATES] = {1, 4};
        anim_obj *anim = anim_create(note_sprite, frametr, n_frames, NOTES_PER_MEASURE / 16, false);
        note->exists = true;
        note->type = 0;
        note->popped = false;
        note->hold = false;
        note->pos.x = 0;
        note->pos.y = 0;
        note->channel = 0;
        note->anim = anim;
    } else {
        note->exists = false;
    }
}

whitgl_ivec get_note_pos(int channel, int offset_idx, int earliest_idx, int latest_idx, float time_since_note) {
    int total_path_len = note_pop_pos.x + 8 + note_pop_pos.y - 8;
    whitgl_ivec pos = {0, 0};
    float note_completion = NOTES_PER_MEASURE * BPM / 60.0f / 4.0f * time_since_note;
    float completion = ((float)(earliest_idx - offset_idx) + note_completion) / ((float)(earliest_idx));
    float ease_completion = completion >= 1.0f ? 1.0f : 1.0f - (completion - 1.0f) * (completion - 1.0f);
    //int x_total_offset = (int)((float)SCREEN_W * (channel + 1.0f) / 5.0f) - note_pop_pos.x;
    int x_total_offset = 0;
    int y_total_offset = -note_pop_pos.y - 16;
    /*int path_pos = (int)(completion * total_path_len);
    if (path_pos < note_pop_pos.x + 8) {
        pos.x = SCREEN_W + 8 - path_pos;
        pos.y = 8;
    } else {
        pos.x = note_pop_pos.x;
        pos.y = path_pos - (note_pop_pos.x + 8) + 8;
    }*/
    pos.x = x_total_offset * (1.0f - completion) + note_pop_pos.x;
    pos.y = y_total_offset * (1.0f - completion) + note_pop_pos.y;
    return pos;
}


void draw_notes(note_t *beat, int cur_note, float time_since_note, int x) {
    int total_notes = NOTES_PER_MEASURE * MEASURES_PER_LOOP;
    int earliest_idx = NOTES_PER_MEASURE / 2;
    int latest_idx = -NOTES_PER_MEASURE / 8;
    for (int i = latest_idx; i < earliest_idx; i++) {
        int note_idx = _mod(cur_note + i, total_notes);
        note_t *note = &beat[note_idx];
        if (note->exists && !note->popped) {
            whitgl_ivec pos = get_note_pos(note->channel, i, earliest_idx, latest_idx, time_since_note);
            pos.x = x;
            note->pos = pos;
            anim_obj *anim = note->anim;
            //whitgl_sys_draw_sprite(anim->sprite, anim->frametr, note->pos);
            whitgl_iaabb note_iaabb = {note->pos, {note->pos.x + 16, note->pos.y + 16}};
            whitgl_sys_color fill = {0, 0, 255, 255};
            whitgl_sys_color outline = {255, 255, 255, 255};
            whitgl_sys_draw_iaabb(note_iaabb, fill);
            whitgl_sys_draw_hollow_iaabb(note_iaabb, 1, outline);
        }
    }
}

