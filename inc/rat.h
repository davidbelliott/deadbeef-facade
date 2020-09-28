#ifndef RAT_H
#define RAT_H

#include "list.h"
#include "anim.h"
#include "map.h"

typedef struct rat_t {
    unsigned int id;
    int notes_between_update;
    int type;
    int difficulty;
    whitgl_ivec pos;
    whitgl_ivec look_pos;
    anim_obj *anim;
    list_t *path;
    int health;
    bool dead;
    bool boss;
} rat_t;

typedef struct player_t player_t;

rat_t* rat_get(int id);
rat_t* rat_create(whitgl_ivec pos, map_t *map);
void rat_kill(int rat_id);
void rat_deal_damage(rat_t *rat, int dmg);
int get_closest_targeted_rat(whitgl_ivec player_pos, whitgl_ivec player_facing, map_t *map);
int get_closest_visible_rat(whitgl_ivec player_pos, map_t *map);
void rats_prune(player_t *p, map_t *m);
void draw_rats(whitgl_fmat view, whitgl_fmat persp);
void rats_on_note(player_t *p, int note, bool use_astar, map_t *map);
void rat_update(int idx);
void rats_update(player_t *p, unsigned int dt, int cur_note, bool use_astar, map_t *map);
void rats_destroy_all(player_t *p, map_t *m);
bool rat_try_move(rat_t *r, whitgl_ivec target, map_t *map);

#endif // RAT_H
