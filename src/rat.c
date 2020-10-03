#include "rat.h"
#include "common.h"
#include "game.h"
#include "graphics.h"
#include "astar.h"
#include "midi.h"
#include "key.h"

#include <whitgl/logging.h>
#include <whitgl/random.h>
#include <stdlib.h>
#include <math.h>

rat_t* rats[MAX_N_RATS] = {0};

rat_t* rat_get(int id) {
    return rats[id];
}

rat_t* rat_create(whitgl_ivec pos, map_t *map, int type, int difficulty) {
    rat_t *rat = NULL;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if(!rats[i]) {
            rat = (rat_t*)malloc(sizeof(rat_t));
            rat->id = i;
            rat->pos = pos;
            rat->look_pos.x = pos.x * 256;
            rat->look_pos.y = pos.y * 256;
            whitgl_sprite rat_sprite = {0, {128,0}, {64,64}};
            whitgl_ivec frametr = {0, 0};
            int n_frames[MAX_N_ANIM_STATES] = {2};
            rat->anim = anim_create(rat_sprite, frametr, n_frames, NOTES_PER_MEASURE / 4, true);
            rat->path = NULL;
            rat->dead = false;
            rat->difficulty = difficulty;
            rat->boss = false;
            switch (type) {
                case ENTITY_TYPE_RUNNER:
                    rat->move = true;
                    rat->notes_between_update = 4;
                    rat->use_astar = false;
                    break;
                case ENTITY_TYPE_CHASER:
                    rat->move = true;
                    rat->notes_between_update = 8;
                    rat->use_astar = true;
                    break;
                case ENTITY_TYPE_BOSS:
                    rat->move = true;
                    rat->notes_between_update = 8;
                    rat->use_astar = true;
                    rat->boss = true;
                    break;
                case ENTITY_TYPE_WALKER:
                    rat->move = true;
                    rat->notes_between_update = 16;
                    rat->use_astar = false;
                    break;
                case ENTITY_TYPE_BLOCKER:
                    rat->move = false;
                    break;
                default:
                case ENTITY_TYPE_RAT:
                    rat->move = true;
                    rat->notes_between_update = 16;
                    rat->use_astar = true;
                    break;
            }
            rat->move_dir.x = 1;
            rat->move_dir.y = 0;
            rat->type = type;

            MAP_SET_ENTITY(map, pos.x, pos.y, ENTITY_TYPE_RAT);
            rats[rat->id] = rat;
            break;
        }
    }
    return rat;
}

static void rat_destroy(rat_t *rat, player_t *p, map_t *m) {
    bool boss = rat->boss;
    whitgl_ivec pos = rat->pos;
    if (p->targeted_rat == rat->id) {
        p->targeted_rat = -1;
    }

    MAP_SET_ENTITY(m, rat->pos.x, rat->pos.y, ENTITY_TYPE_NONE);
    rats[rat->id] = NULL;
    free(rat);

    if (boss) {
        MAP_SET_ENTITY(m, pos.x, pos.y, ENTITY_TYPE_BKEY);
        key_create(pos, KEY_B, m);
    }
}

void rat_kill(int rat_id) {
    rats[rat_id]->dead = true;
}

void rats_prune(player_t *p, map_t *m) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i] && rats[i]->dead) {
            rat_destroy(rats[i], p, m);
        }
    }
}

void draw_rats(whitgl_fmat view, whitgl_fmat persp) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            int y_offset = 0;
            switch (rats[i]->type) {
                case ENTITY_TYPE_BLOCKER:
                    y_offset = 64;
                    break;
                default:
                    break;
            }
            whitgl_fvec rat_offset = {128 + 64 * rats[i]->anim->frametr.x,
                y_offset};
            //whitgl_fvec rat_offset = {188, 87};
            whitgl_fvec rat_size = {64, 64};
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 2, rat_offset);
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 3, rat_size);
            whitgl_fvec3 pos = {(float)rats[i]->look_pos.x / 256.0f + 0.5f, (float)rats[i]->look_pos.y / 256.0f + 0.5f, 0.5f};
            draw_billboard(pos, view, persp);
        }
    }
}

double diag_distance(whitgl_ivec start, whitgl_ivec end) {
    double max = MAX(abs(start.x - end.x), abs(start.y - end.y));
    double min = MIN(abs(start.x - end.x), abs(start.y - end.y));
    return (max - min) + SQRT2 * min;
}

bool rat_try_move(rat_t *r, whitgl_ivec target, map_t *map) {
    if (!tile_walkable[MAP_TILE(map, target.x, target.y)] || 
            MAP_ENTITY(map, target.x, target.y) != ENTITY_TYPE_NONE) {
        return false;
    }
    MAP_SET_ENTITY(map, r->pos.x, r->pos.y, ENTITY_TYPE_NONE);
    MAP_SET_ENTITY(map, target.x, target.y, ENTITY_TYPE_RAT);
    r->pos = target;
    return true;
}

void rat_find_path(rat_t *r, whitgl_ivec start, whitgl_ivec end, map_t *map) {
    r->path = astar(start, end, map);
    if (r->path) {
        astar_node_t *start_node = pop_front(&r->path);
        free(start_node);
    }
}

void rat_follow_path(struct rat_t *r, struct player_t *p, map_t *map,
        int note) {
    // Follow whatever path the rat has found, if can't see player
    if (r->path) {
        astar_node_t *next_node = (astar_node_t*)r->path->data;
        whitgl_ivec target = {next_node->pt.x, next_node->pt.y};
        WHITGL_LOG("Following path -> (%d, %d)", target.x, target.y);
        if (rat_try_move(r, target, map)) {
            astar_node_t *reached_node = pop_front(&r->path);
            free(reached_node);
        } else {
            if (MAP_ENTITY(map, target.x, target.y) == ENTITY_TYPE_PLAYER)
                player_deal_damage(p, RAT_DAMAGE, note);
            astar_free_node_list(&r->path);
            r->path = NULL;
        }
    }
}

static whitgl_ivec rrot(whitgl_ivec v) {
    int x = v.x;
    int y = v.y;
    v.x = y;
    v.y = -x;
    return v;
}

void rat_step(struct rat_t *r, struct player_t *p, int note, map_t *map) {
    if (r->use_astar) {
        // If within astar range, find a path
        if (diag_distance(p->pos, r->pos) < 20.0) {
            whitgl_ivec start = {(int)(r->pos.x), (int)(r->pos.y)};
            whitgl_ivec end = {(int)(p->pos.x), (int)(p->pos.y)};
            rat_find_path(r, start, end, map);
        }
        rat_follow_path(r, p, map, note);
    } else if (r->move) {
        WHITGL_LOG("moving");
        whitgl_ivec next_pos = whitgl_ivec_add(r->pos, r->move_dir);
        for (int i = 0; i < 4 && !rat_try_move(r, next_pos, map); i++) {
            if (MAP_ENTITY(map, next_pos.x, next_pos.y) == ENTITY_TYPE_PLAYER)
                player_deal_damage(p, RAT_DAMAGE, note);
                r->move_dir = rrot(r->move_dir);
                next_pos = whitgl_ivec_add(r->pos, r->move_dir);
        }
    }
}

void rats_on_note(struct player_t *p, int note, bool use_astar, map_t *map) {
    for(int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i] && rats[i]->notes_between_update != 0
                    && note % rats[i]->notes_between_update == 0) {
            rat_step(rats[i], p, note, map);
        }
    }
}

void rat_update(int rat_idx) {
    if (rats[rat_idx]) {
        rat_t *r = rats[rat_idx];
        if (r->pos.x * 256 > r->look_pos.x) {
            r->look_pos.x += 32;
        } else if (r->pos.x * 256 < r->look_pos.x) {
            r->look_pos.x -= 32;
        }
        if (r->pos.y * 256 > r->look_pos.y) {
            r->look_pos.y += 32;
        } else if (r->pos.y * 256 < r->look_pos.y) {
            r->look_pos.y -= 32;
        }
    }
}

void rats_update(struct player_t *p, unsigned int dt, int cur_note, bool use_astar, map_t *map) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        rat_update(i);
    }
}

int get_closest_targeted_rat(whitgl_ivec player_pos, whitgl_ivec player_facing, map_t *map) {
    int closest_hit_rat = -1;
    whitgl_ivec test_pos = player_pos;
    for (int i = 0; i < MAX_DIST_TO_TARGET_RAT; i++) {
        test_pos = whitgl_ivec_add(test_pos, player_facing);
        if (MAP_ENTITY(map, test_pos.x, test_pos.y) == ENTITY_TYPE_RAT) {
            for (int i = 0; i < MAX_N_RATS; i++) {
                if (rats[i] && whitgl_ivec_eq(rats[i]->pos, test_pos)) {
                    return i;
                }
            }
        }
    }
    return -1;
}

void rats_destroy_all(player_t *p, map_t *m) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            rat_destroy(rats[i], p, m);
        }
    }
}
