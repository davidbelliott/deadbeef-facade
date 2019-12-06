#include "common.h"

#include <math.h>

int _mod(int a, int b) {
    int r = a % b;
    return r < 0 ? r + b : r;
}

float _fmod(float a, float b) {
    float r = fmod(a, b);
    return r < 0.0f ? r + b : r;
}

int _sign(double x) {
	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 1;
	} else {
		return 0;
	}
}

int tile_lvl_rgb[N_TILE_TYPES][4] = {
    {0, 0, 0},
    {0, 0, 127},
    {127, 0, 0},
    {0, 0, 255},
    {0, 255, 255},
};

int tile_tex_offset[N_TILE_TYPES][2] = {
    {0, 0},
    {0, 0},
    {256, 0},
    {320, 0},
    {384, 0}
};

bool tile_walkable[N_TILE_TYPES] = {
    true,
    false,
    false,
    false,
    true
};

void draw_window(char *title, whitgl_iaabb iaabb, whitgl_sys_color fill) {
    whitgl_sys_draw_iaabb(iaabb, fill);
    whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
    whitgl_ivec text_pos = {iaabb.a.x, iaabb.a.y};
    whitgl_sys_draw_text(font, title, text_pos);
}

