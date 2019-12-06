#ifndef COMMON_H
#define COMMON_H

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
#define NOTES_PER_MEASURE 96
#define MEASURES_PER_LOOP 4
#define TOTAL_NUM_NOTES (NOTES_PER_MEASURE * MEASURES_PER_LOOP)
#define NUM_LOOPS 16
#define BPM 127.0

#define FONT_CHAR_W 9
#define FONT_CHAR_H 16

#define AMBIENT_MUSIC -1

#define SHORT_TEXT_MAX_LEN 256

#define N_TILE_TYPES 4


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

#endif
