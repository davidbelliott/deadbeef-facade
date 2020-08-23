#ifndef COMMON_H
#define COMMON_H

#include <whitgl/sys.h>
#include <stdbool.h>

enum {
	MAP_WIDTH	= 64,
	MAP_HEIGHT	= 64,
	CELL_SIZE	= 4,
};

enum {
    TEX_WALL = 0,
    TEX_FLOOR = 1,
    TEX_RAT = 2,
    TEX_OVERLAY = 3
};

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define SCREEN_W 640
#define SCREEN_H 480
#define PIXEL_DIM 1
#define MOUSE_SENSITIVITY 2.0
#define MAX_DIST 15
#define MOVE_SPEED 3.000
#define RAT_SPEED MOVE_SPEED * 0.6
#define RAT_HITBOX_RADIUS 0.5f
#define RAT_ATTACK_RADIUS 0.75f
#define MAX_DIST_TO_TARGET_RAT 5
#define RAT_ATTACK_NOTES 48
#define RAT_DAMAGE 10
#define MAX_N_RATS 256
#define MAX_N_PHYS 1024
#define MAX_N_PHYS_PASSES 8
#define MAX_N_PATHFINDING_STEPS 256
#define MAX_N_ANIM_STATES 8
#define MAX_N_ANIM_OBJS 1024 * 128
#define CROSSHAIR_DIAM 9
#define SQRT2 1.41421356237
#define NOTES_PER_MEASURE 32
#define MEASURES_PER_LOOP 4
#define TOTAL_NUM_NOTES (NOTES_PER_MEASURE * MEASURES_PER_LOOP)
#define NUM_LOOPS 16
#define BPM 130

#define FONT_CHAR_W 9
#define FONT_CHAR_H 16


#define SHORT_TEXT_MAX_LEN 256

typedef struct point2 {
	double x;
	double y;
} point2;

typedef struct ipoint2 {
    int x;
    int y;
} ipoint2;

typedef struct vector2 {
	double x;
	double y;
} vector2;

int _mod(int a, int b);
float _fmod(float a, float b);
int _sign(double x);
void draw_window(char *title, whitgl_iaabb iaabb, whitgl_sys_color fill);

void draw_str_with_newlines(const char *text, int n_chars, whitgl_ivec pos);
void wrap_text(const char *text, char *wrapped, int wrapped_len, whitgl_iaabb bounding_box);

enum {
    GAME_STATE_GAME = 0,
    GAME_STATE_MENU = 1,
    GAME_STATE_INTRO = 2,
    GAME_STATE_MIDI = 3
};

#define TILE_TYPE_FLOOR     0
#define TILE_TYPE_BBRICK    1
#define TILE_TYPE_RBRICK    2
#define TILE_TYPE_STONE     3
#define TILE_TYPE_PORTAL    4
#define N_TILE_TYPES        5

#define ENTITY_TYPE_NONE    0
#define ENTITY_TYPE_PLAYER  1
#define ENTITY_TYPE_RAT     2
#define N_ENTITY_TYPES      3

extern int tile_lvl_rgb[N_TILE_TYPES][4];
extern int tile_tex_offset[N_TILE_TYPES][2];
extern bool tile_walkable[N_TILE_TYPES];

#endif

