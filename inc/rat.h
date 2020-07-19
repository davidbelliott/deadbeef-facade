#ifndef RAT_H
#define RAT_H

#include "physics.h"
#include "list.h"
#include "anim.h"
#include "note.h"
#include "map.h"

typedef struct rat_t {
    unsigned int id;
    int type;
    whitgl_ivec pos;
    whitgl_ivec look_pos;
    anim_obj *anim;
    list_t *path;
    int health;
    bool dead;
    note_t beat[NOTES_PER_MEASURE * MEASURES_PER_LOOP];
} rat_t;

typedef struct player_t player_t;

rat_t* rat_get(int id);
rat_t* rat_create(whitgl_ivec pos);
void rat_destroy(rat_t *rat, player_t *p);
void rat_deal_damage(rat_t *rat, int dmg);
int get_closest_targeted_rat(whitgl_ivec player_pos, whitgl_fvec player_look, map_t *map);
int get_closest_visible_rat(whitgl_ivec player_pos, map_t *map);
void rats_prune(player_t *p);
void draw_rats(whitgl_fmat view, whitgl_fmat persp);
void rats_update(player_t *p, unsigned int dt, int cur_note, bool use_astar, map_t *map);
void rats_destroy_all(player_t *p);

#endif // RAT_H
