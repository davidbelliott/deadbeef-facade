#include <whitgl/sound.h>
#include <whitgl/logging.h>
#include <stdio.h>

#include "music.h"
#include "common.h"

whitgl_float song_time = 0.0f;
whitgl_float offset_time = 0.23f;

int actual_song_millis = 0;
int prev_actual_song_millis = 0;
int cur_snd = 0;
double cur_bpm = -1.0;

void music_init() {
}

void music_set_paused(int snd, bool paused_in) {
    whitgl_loop_set_paused(snd, paused_in);
}

void music_play_from_beginning(int snd, double bpm) {
    cur_snd = snd;
    cur_bpm = bpm;
    song_time = 0.0f;
    actual_song_millis = 0;
    prev_actual_song_millis = 0;
    whitgl_loop_seek(snd, 0.0f);
    //whitgl_loop_volume(cur_snd, 0.0f);
    whitgl_loop_set_paused(snd, false);
}

void music_update(float dt) {
    // Update song time
    float song_len = whitgl_loop_get_length(cur_snd) / 1000.0f;
    song_time = whitgl_fmod(song_time + dt, song_len);

    actual_song_millis = whitgl_loop_tell(cur_snd);
    if (actual_song_millis != prev_actual_song_millis) {
        float actual_song_time = actual_song_millis / 1000.0f;
        float diff_ff, diff_rr;
        if (actual_song_time > song_time) {
            diff_ff = actual_song_time - song_time;
            diff_rr = song_time + song_len - actual_song_time;
        } else {
            diff_ff = actual_song_time + song_len - actual_song_time;
            diff_rr = song_time - actual_song_time;
        }
        if (diff_ff < diff_rr) {
            song_time += diff_ff / 2.0f;
        } else {
            song_time -= diff_rr / 2.0f;
        }
        song_time = whitgl_fmod(song_time, song_len);
        prev_actual_song_millis = actual_song_millis;
    }

    // Update volume
    //whitgl_loop_volume(cur_snd, whitgl_fmin(song_time / 5.0f, 1.0f));
}

float music_get_song_time() {
    return song_time;
}

// get the length of the song in notes (32nd notes)
int music_get_song_len() {
    float secs_per_note = (60.0f / cur_bpm * 4 / NOTES_PER_MEASURE);
    float song_len = whitgl_loop_get_length(cur_snd) / 1000.0f;
    return (int)(song_len / secs_per_note);
}

// div=1 means 32nd notes, 2 means 16th notes...
int music_get_cur_note() {
    float secs_per_note = (60.0f / cur_bpm * 4 / NOTES_PER_MEASURE);
    int cur_note = (int)((song_time - offset_time)/ secs_per_note);
    return cur_note;
}

float music_get_time_since_note() {
    float secs_per_note = (60.0f / cur_bpm * 4 / NOTES_PER_MEASURE);
    return _fmod(song_time - offset_time, secs_per_note);
}

float music_get_frac_since_note() {
    float secs_per_note = (60.0f / cur_bpm * 4 / NOTES_PER_MEASURE);
    return music_get_time_since_note() / secs_per_note;
}

double music_get_cur_bpm() {
    return cur_bpm;
}
