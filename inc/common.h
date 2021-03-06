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
#define SCREEN_W 683
#define SCREEN_H 384
//#define SCREEN_W 640
//#define SCREEN_H 480
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
#define MAX_N_ANIM_OBJS 128
#define CROSSHAIR_DIAM 9
#define SQRT2 1.41421356237
#define NOTES_PER_MEASURE 32
#define MEASURES_PER_LOOP 4
#define TOTAL_NUM_NOTES (NOTES_PER_MEASURE * MEASURES_PER_LOOP)
#define NUM_LOOPS 16

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
void draw_text(char *text, whitgl_ivec pos);
void draw_window(char *title, whitgl_iaabb iaabb, whitgl_sys_color fill);

void draw_str_with_newlines(const char *text, int n_chars, whitgl_ivec pos);
void wrap_text(const char *text, char *wrapped, int wrapped_len, whitgl_iaabb bounding_box);

enum {
    GAME_STATE_GAME,
    GAME_STATE_MENU,
    GAME_STATE_INTRO,
    GAME_STATE_MIDI,
    GAME_STATE_PAUSE,
    GAME_STATE_ENDING,
    GAME_STATE_EXIT,
};

#define TILE_TYPE_FLOOR     0
#define TILE_TYPE_BBRICK    1
#define TILE_TYPE_RBRICK    2
#define TILE_TYPE_STONE     3
#define TILE_TYPE_PORTAL    4
#define TILE_TYPE_RDOOR     5
#define TILE_TYPE_GDOOR     6
#define TILE_TYPE_BDOOR     7
#define TILE_TYPE_RFINE     8
#define TILE_TYPE_BFINE     9
#define N_TILE_TYPES        10

enum {
    ENTITY_TYPE_NONE,
    ENTITY_TYPE_PLAYER,
    ENTITY_TYPE_RKEY,
    ENTITY_TYPE_GKEY,
    ENTITY_TYPE_BKEY,
    ENTITY_TYPE_RAT,
    ENTITY_TYPE_BOSS,
    ENTITY_TYPE_BLOCKER,
    ENTITY_TYPE_WALKER,
    ENTITY_TYPE_RUNNER,
    ENTITY_TYPE_CHASER,
    N_ENTITY_TYPES
};

extern int tile_lvl_rgb[N_TILE_TYPES][3];
extern int entity_lvl_rgb[N_ENTITY_TYPES][3];
extern int tile_tex_offset[N_TILE_TYPES][2];
extern bool tile_walkable[N_TILE_TYPES];

#endif

