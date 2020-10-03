#include "common.h"
#include "music.h"
#include "ending.h"

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PER_CHAR_DELAY 0.02f

static char *text = NULL;

static float elapsed_time;
static int text_chars;
static int next_gamestate;
static bool waiting_to_exit;
static int level;

void ending_init() {
}

void ending_cleanup() {
    if (text) {
        free(text);
        text = NULL;
    }
}

void ending_start() {
    elapsed_time = 0.0f;
    text_chars = 0;
    next_gamestate = GAME_STATE_MENU;
    waiting_to_exit = false;
}

int ending_update(float dt) {
    elapsed_time += dt;
    int new_text_chars = text_chars;
    if (text_chars != strlen(text)) {
        new_text_chars = MIN(strlen(text), elapsed_time / PER_CHAR_DELAY);
    }
    if (new_text_chars != text_chars) {
        //whitgl_sound_play(0, 1, 1);
    }
    text_chars = new_text_chars;
    int cur_note = music_get_cur_note();
    if (waiting_to_exit) {
        return GAME_STATE_MENU;
    } else {
        return GAME_STATE_ENDING;
    }
}

void ending_input() {
    if (whitgl_input_pressed(WHITGL_INPUT_A)) {
        if (text_chars != strlen(text)) {
            text_chars = strlen(text);
        } else {
            waiting_to_exit = true;
        }
    }
}

void ending_frame() {
    whitgl_sys_draw_init(0);
    whitgl_sys_enable_depth(false);

    whitgl_sys_color col = {0, 0, 128, 255};
    whitgl_iaabb outer_box = {{0, 0}, {SCREEN_W, SCREEN_H}};
    draw_window("", outer_box, col);

    whitgl_iaabb iaabb = {{FONT_CHAR_W, 0}, {SCREEN_W, FONT_CHAR_H}};
    whitgl_sys_color black = {0, 0, 0, 255};
    char path[256];
    snprintf(path, 256, "TXT: data/lvl/lvl%d/text.txt", level);
    draw_window(path, iaabb, col);

    whitgl_iaabb img_iaabb = {{SCREEN_W - 255 - FONT_CHAR_W, 0}, {SCREEN_W, FONT_CHAR_H}};
    char img[256];
    snprintf(img, 256, "IMG: data/lvl/lvl%d/img.bmp", level);
    draw_window(img, img_iaabb, col);

    whitgl_iaabb bounding_box = {{FONT_CHAR_W, FONT_CHAR_H}, {SCREEN_W - 2 * FONT_CHAR_W - 256, SCREEN_H - FONT_CHAR_H}};
    draw_window("", bounding_box, black);
    int wrapped_len = strlen(text) + SCREEN_H / FONT_CHAR_H;    // strlen + max_newlines + 1
    char *wrapped_text = (char*)malloc(wrapped_len);
    wrap_text(text, wrapped_text, wrapped_len, bounding_box);
    draw_str_with_newlines(wrapped_text, text_chars, bounding_box.a);
    free(wrapped_text);

    whitgl_sys_draw_finish();
}

void ending_pause(bool paused) {

}

void ending_set_text(char *newtext) {
    if (text) {
        free(text);
        text = NULL;
    }
    text = malloc(sizeof(char) * (strlen(newtext) + 1));
    strcpy(text, newtext);
}
