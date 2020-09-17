#include "common.h"
#include "pause.h"
#include "music.h"
#include <whitgl/sys.h>
#include <whitgl/input.h>

#define N_SELECTIONS 2

static int next_gamestate;
static int selection;

void pause_init() {
}

void pause_cleanup() {

}

void pause_start() {
    next_gamestate = GAME_STATE_PAUSE;
    selection = 0;
}

int pause_update(float dt) {
    return next_gamestate;
}

void pause_input() {
    if (whitgl_input_pressed(WHITGL_INPUT_A)) {
        switch (selection) {
            case 0:
                next_gamestate = GAME_STATE_GAME;
                break;
            case 1:
                next_gamestate = GAME_STATE_MENU;
                break;
            default:
                break;
        }
    }
    if (whitgl_input_pressed(WHITGL_INPUT_DOWN)) {
        selection = (selection + 1) % N_SELECTIONS;
    }
    if (whitgl_input_pressed(WHITGL_INPUT_UP)) {
        selection = selection > 0 ? selection - 1 : N_SELECTIONS - 1;
    }
}

void pause_frame() {
    whitgl_sys_draw_init(0);
    whitgl_sys_enable_depth(false);
    
    whitgl_sys_color col = {0, 0, 128, 255};
    whitgl_iaabb outer_box = {{0, 0}, {SCREEN_W, SCREEN_H}};
    draw_window("", outer_box, col);

    whitgl_iaabb iaabb = {{FONT_CHAR_W, 0}, {SCREEN_W, FONT_CHAR_H}};
    whitgl_sys_color black = {0, 0, 0, 255};
    draw_window("0xDEADBEEF_FACADE", iaabb, col);

    whitgl_iaabb bounding_box = {{FONT_CHAR_W, FONT_CHAR_H}, {SCREEN_W - FONT_CHAR_W, SCREEN_H - FONT_CHAR_H}};
    draw_window("", bounding_box, black);

    whitgl_ivec pos = {FONT_CHAR_W * 2, FONT_CHAR_H * 2};
    whitgl_ivec offs = {0, FONT_CHAR_H};
    whitgl_ivec cursor_pos = {FONT_CHAR_W, FONT_CHAR_H * (2 + selection)};
    draw_text(">", cursor_pos);
    draw_text("Resume", pos);
    pos = whitgl_ivec_add(pos, offs);
    draw_text("Main Menu", pos);

    /*whitgl_sprite main_sprite = {IMG_TITLE, {0, 0}, {256, 128}};
    whitgl_ivec frametr = {0, 0};
    whitgl_ivec pos = {64, 64};
    whitgl_sys_draw_sprite(main_sprite, frametr, pos);*/


    whitgl_sys_draw_finish();
}
