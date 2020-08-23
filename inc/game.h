#ifndef GAME_H
#define GAME_H

#define PLAYER_ANGLE_PI_FRAC 128

#include <stdbool.h>
#include <whitgl/sys.h>

typedef struct player_t {
    whitgl_ivec pos;
    whitgl_ivec look_pos;   // divide by 256 to get apparent pos
    int angle;
    int look_angle;
    int move;
    whitgl_fvec look_facing;
    whitgl_ivec facing;
    int last_rotate_dir;
    int health;
    int last_damage;
    int targeted_rat;
    bool moved;
    int move_time;
    int move_goodness;
} player_t;

void player_deal_damage(player_t *p, int dmg);

void game_init();
void game_from_midi();
void game_cleanup();
// Action to take when entering this from another state
void game_start();
// Action to take when leaving this for another state
void game_stop();
int game_update(float dt);
void game_input();
void game_frame();
void game_pause(bool paused);
void notify(const char *str, whitgl_sys_color color);

#endif
