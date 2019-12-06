#ifndef GAME_H
#define GAME_H

#include "physics.h"

#include <stdbool.h>

typedef struct player_t {
    physics_obj *phys;
    double angle;
    whitgl_fvec look_direction;
    whitgl_fvec move_direction;
    int health;
    float damage_severity;
    int targeted_rat;
} player_t;

void player_deal_damage(player_t *p, int dmg);

void game_init();
void game_cleanup();
void game_start();
int game_update(float dt);
void game_input();
void game_frame();
void game_pause(bool paused);

#endif
