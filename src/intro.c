#include "common.h"

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include <string.h>
#include <stdlib.h>

#define PER_CHAR_DELAY 0.02f

const char text[] = "Sometimes a man gets mogged so hard he forgets to breathe. When life gives you a moment, you have to live in the moment. You're not Lindy, hunched by your bed typing into Vim. A man's ------ up on dark mode solarized cool command line life hacks, even though dark mode's for -------. Is a man meant to live life staring into a dark grotto, or with his eyes trained on the bright savanna horizon scanning for prey?\n\nA man's gotta make a monument, a monument more lasting than bronze. A man dreams of saying on his deathbed, \"I knew I had it in me all along.\" How bitter it will be to admit you were wrong.";

float elapsed_time;
int text_chars;
int next_gamestate;

static void draw_str_with_newlines(const char *text, int n_chars, whitgl_ivec pos) {
    char *line = (char*)malloc(n_chars + 1);
    whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
    int x = pos.x;
    int y = pos.y;
    int line_idx = 0;
    for (int i = 0; i < n_chars > 0; i++) {
        if (text[i] == '\n') {
            line[line_idx] = '\0';
            whitgl_ivec text_pos = {pos.x, y};
            whitgl_sys_draw_text(font, line, text_pos);
            x = pos.x;
            y += FONT_CHAR_H;
            line_idx = 0;
        } else {
            line[line_idx++] = text[i];
        }
    }
    line[line_idx] = '\0';
    whitgl_ivec text_pos = {pos.x, y};
    whitgl_sys_draw_text(font, line, text_pos);
    free(line);
}

static void wrap_text(const char *text, char *wrapped, int wrapped_len, whitgl_iaabb bounding_box) {
    int len = strlen(text);
    int x = bounding_box.a.x;
    int y = bounding_box.a.y;
    int max_x = bounding_box.b.x;
    int max_y = bounding_box.b.y;
    int last_whitespace_idx = -1;
    int wrapped_idx = 0;
    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            // new line
            x = bounding_box.a.x;
            y += FONT_CHAR_H;
            last_whitespace_idx = -1;
        }
        if (text[i] == ' ') {
            last_whitespace_idx = wrapped_idx;
        }
        if (x + FONT_CHAR_W <= max_x) {
            wrapped[wrapped_idx++] = text[i];   // copy char to wrapped
            x += FONT_CHAR_W;
        } else {
            // Must wrap
            if (last_whitespace_idx != -1) {
                // Whitespace on current line; replace with newline
                wrapped[last_whitespace_idx] = '\n';
                i -= (wrapped_idx - last_whitespace_idx);
                wrapped_idx = last_whitespace_idx + 1;
            } else {
                // No whitespace; must insert newline between letters
                wrapped[wrapped_idx++] = '\n';
                i -= 1;
            }
            x = bounding_box.a.x;
            y += FONT_CHAR_H;
            last_whitespace_idx = -1;
        }
        if (wrapped_idx == wrapped_len - 1) // ran out of space
            break;
    }
    wrapped[wrapped_idx] = '\0';
}

void intro_init() {
}

void intro_cleanup() {

}

void intro_start() {
    elapsed_time = 0.0f;
    text_chars = 0;
    next_gamestate = 2;
}

int intro_update(float dt) {
    elapsed_time += dt;
    int new_text_chars = text_chars;
    if (text_chars != strlen(text)) {
        new_text_chars = MIN(strlen(text), elapsed_time / PER_CHAR_DELAY);
    }
    if (new_text_chars != text_chars) {
        //whitgl_sound_play(0, 1, 1);
    }
    text_chars = new_text_chars;
    WHITGL_LOG("elapsed: %f", elapsed_time);
    return next_gamestate;
}

void intro_input() {
    if (whitgl_input_pressed(WHITGL_INPUT_A)) {
        if (text_chars != strlen(text)) {
            text_chars = strlen(text);
        } else {
            next_gamestate = 0;
        }
    }
}

void intro_frame() {
    whitgl_sys_draw_init(0);
    whitgl_sys_enable_depth(false);
    whitgl_iaabb bounding_box = {{16, 16}, {SCREEN_W - 16, SCREEN_H - 16}};
    int wrapped_len = strlen(text) + SCREEN_H / FONT_CHAR_H;    // strlen + max_newlines + 1
    char *wrapped_text = (char*)malloc(wrapped_len);
    wrap_text(text, wrapped_text, wrapped_len, bounding_box);
    draw_str_with_newlines(wrapped_text, text_chars, bounding_box.a);
    free(wrapped_text);
    whitgl_sys_draw_finish();
}

void intro_pause(bool paused) {

}
