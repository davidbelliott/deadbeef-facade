#ifndef GAME_H
#define GAME_H

#define PLAYER_ANGLE_PI_FRAC 128

#include "physics.h"

#include <stdbool.h>

typedef struct player_t {
    physics_obj *phys;
    int angle;
    int look_angle;
    whitgl_float move_dir;
    whitgl_fvec look_direction;
    whitgl_fvec move_direction;
    int last_rotate_dir;
    int health;
    float damage_severity;
    int targeted_rat;
} player_t;

void player_deal_damage(player_t *p, int dmg);

void game_init();
void game_cleanup();
// Action to take when entering this from another state
void game_start();
// Action to take when leaving this for another state
void game_stop();
int game_update(float dt);
void game_input();
void game_frame();
void game_pause(bool paused);

#endif
