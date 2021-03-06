#include "common.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

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

int tile_lvl_rgb[][3] = {
    {0, 0, 0},      // TILE_TYPE_FLOOR
    {0, 0, 127},    // TILE_TYPE_BBRICK
    {127, 0, 0},    // TILE_TYPE_RBRICK
    {0, 127, 255},  // TILE_TYPE_STONE
    {0, 255, 255},  // TILE_TYPE_PORTAL
    {255, 0, 0},    // TILE_TYPE_RDOOR
    {0, 255, 0},    // TILE_TYPE_GDOOR
    {0, 0, 255},    // TILE_TYPE_BDOOR
    {0, 127, 127},  // TILE_TYPE_RFINE
    {127, 127, 255},  // TILE_TYPE_BFINE
};

int entity_lvl_rgb[N_ENTITY_TYPES][3] = {
    {0, 0, 0},          // ENTITY_TYPE_NONE
    {255, 255, 255},    // ENTITY_TYPE_PLAYER
    {255, 0, 127},      // ENTITY_TYPE_RKEY
    {0, 127, 0},        // ENTITY_TYPE_GKEY
    {127, 0, 255},      // ENTITY_TYPE_BKEY
    {255, 255, 0},      // ENTITY_TYPE_RAT
    {255, 127, 0},      // ENTITY_TYPE_BOSS
    {255, 127, 127},    // ENTITY_TYPE_BLOCKER
    {127, 127, 0},      // ENTITY_TYPE_WALKER
    {127, 255, 0},      // ENTITY_TYPE_RUNNER
    {127, 0, 255},      // ENTITY_TYPE_CHASER
};

int tile_tex_offset[N_TILE_TYPES][2] = {
    {0, 0},         // TILE_TYPE_FLOOR
    {0, 0},         // TILE_TYPE_BBRICK
    {256, 0},       // TILE_TYPE_RBRICK
    {320, 0},       // TILE_TYPE_STONE
    {384, 0},       // TILE_TYPE_PORTAL
    {384, 0},       // TILE_TYPE_RDOOR
    {448, 0},       // TILE_TYPE_GDOOR
    {512, 0},       // TILE_TYPE_BDOOR
    {256, 64},      // TILE_TYPE_RFINE
    {320, 64},      // TILE_TYPE_BFINE
};

bool tile_walkable[N_TILE_TYPES] = {
    true,
    false,
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false
};

void draw_text(char *text, whitgl_ivec pos) {
    whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
    whitgl_sys_draw_text(font, text, pos);
}

void draw_window(char *title, whitgl_iaabb iaabb, whitgl_sys_color fill) {
    whitgl_sys_draw_iaabb(iaabb, fill);
    whitgl_ivec text_pos = {iaabb.a.x, iaabb.a.y};
    draw_text(title, text_pos);
}

void draw_str_with_newlines(const char *text, int n_chars, whitgl_ivec pos) {
    char *line = (char*)malloc(n_chars + 1);
    whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
    int y = pos.y;
    int line_idx = 0;
    for (int i = 0; i < n_chars && text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            line[line_idx] = '\0';
            whitgl_ivec text_pos = {pos.x, y};
            whitgl_sys_draw_text(font, line, text_pos);
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

void wrap_text(const char *text, char *wrapped, int wrapped_len, whitgl_iaabb bounding_box) {
    int len = strlen(text);
    int x = bounding_box.a.x;
    int y = bounding_box.a.y;
    int max_x = bounding_box.b.x;
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

