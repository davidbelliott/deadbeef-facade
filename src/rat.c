#include "rat.h"
#include "common.h"
#include "game.h"
#include "astar.h"

#include <whitgl/logging.h>
#include <stdlib.h>
#include <math.h>

rat_t* rats[MAX_N_RATS] = {0};

rat_t* rat_get(int id) {
    return rats[id];
}

rat_t* rat_create(whitgl_ivec pos) {
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
            rat->health = 50;
            rat->dead = false;

            for (int j = 0; j < NOTES_PER_MEASURE * MEASURES_PER_LOOP; j++) {
                note_create(&rat->beat[j], rand() % 2 == 0 && j % 8 == 0);
            }
            rats[rat->id] = rat;
            break;
        }
    }
    return rat;
}

void rat_destroy(rat_t *rat, player_t *p) {
    if (p->targeted_rat == rat->id) {
        p->targeted_rat = -1;
    }
    rats[rat->id] = NULL;
    free(rat);
}

void rat_deal_damage(rat_t *rat, int dmg) {
    rat->health = MAX(0, rat->health - dmg);
    if (rat->health == 0) {
        rat->dead = true;
    }
}

void rats_prune(player_t *p) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i] && rats[i]->dead) {
            rat_destroy(rats[i], p);
        }
    }
}

static void draw_billboard(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(2, WHITGL_SHADER_EXTRA_1, model_matrix, view, persp);
}

void draw_rats(whitgl_fmat view, whitgl_fmat persp) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            //whitgl_fvec rat_offset = {128 + 64 * rats[i]->anim->frametr.x, 0};
            whitgl_fvec rat_offset = {188, 87};
            whitgl_fvec rat_size = {128, 128};
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 2, rat_offset);
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 3, rat_size);
            whitgl_fvec3 pos = {(float)rats[i]->look_pos.x / 256.0f, (float)rats[i]->look_pos.y / 256.0f, 0.5f};
            draw_billboard(pos, view, persp);
        }
    }
}

bool line_of_sight_exists(whitgl_fvec p1, whitgl_fvec p2, map_t *map) {
    double next_x, next_y;
    double step_x, step_y;
    double max_x, max_y;
    double ray_angle;
    double dx, dy;
    int x, y;

    whitgl_fvec ray_direction = whitgl_fvec_normalize(whitgl_fvec_sub(p2, p1));

    x = p1.x;
    y = p1.y;

    step_x = _sign(ray_direction.x);
    step_y = _sign(ray_direction.y);

    next_x = x + (step_x > 0 ? 1 : 0);
    next_y = y + (step_y > 0 ? 1 : 0);

    max_x = (next_x - p1.x) / ray_direction.x;
    max_y = (next_y - p1.y) / ray_direction.y;

    if (isnan(max_x))
            max_x = INFINITY;
    if (isnan(max_y))
            max_y = INFINITY;

    dx = step_x / ray_direction.x;
    dy = step_y / ray_direction.y;

    if (isnan(dx))
            dx = INFINITY;
    if (isnan(dy))
            dy = INFINITY;

    while(x != (int)p2.x || y != (int)p2.y) {
            if (MAP_GET(map, x, y) != 0) {
                return false;
            }

            if (max_x < max_y) {
                    max_x += dx;
                    x += step_x;
            } else {
                    if(max_y < MAX_DIST) {
                        max_y += dy;
                        y += step_y;
                    } else {
                        break;
                    }
            }
    }
    return true;
}

double diag_distance(whitgl_ivec start, whitgl_ivec end) {
    double max = MAX(abs(start.x - end.x), abs(start.y - end.y));
    double min = MIN(abs(start.x - end.x), abs(start.y - end.y));
    return (max - min) + SQRT2 * min;
}

bool unobstructed(whitgl_ivec p1, whitgl_ivec p2, map_t *map) {
    //if(fabs(p1->x - p2->x) < 1.0 && fabs(p1->y - p2->y) < 1.0)
    //   return true;
    float w = 0.5f;
    whitgl_fvec fp1 = {(float)p1.x, (float)p1.y};
    whitgl_fvec fp2 = {(float)p2.x, (float)p2.y};
    whitgl_fvec v = whitgl_fvec_normalize(whitgl_fvec_sub(fp2, fp1));
    float dx = v.x;
    float dy = v.y;
    whitgl_fvec p11 = {fp1.x - dy * w, fp1.y + dx * w};
    whitgl_fvec p21 = {fp2.x - dy * w, fp2.y + dx * w};
    whitgl_fvec p12 = {fp1.x + dy * w, fp1.y - dx * w};
    whitgl_fvec p22 = {fp2.x + dy * w, fp2.y - dx * w};
    return line_of_sight_exists(p11, p21, map) && line_of_sight_exists(p12, p22, map);
}

void rats_update(struct player_t *p, unsigned int dt, int cur_note, bool use_astar, map_t *map) {
    for(int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            bool line_of_sight = unobstructed(rats[i]->pos, p->pos, map);
            // Deal damage if close enough
            if (line_of_sight
                    && whitgl_ivec_sqmagnitude(whitgl_ivec_sub(rats[i]->pos, p->pos)) <= RAT_ATTACK_RADIUS*RAT_ATTACK_RADIUS
                    && cur_note % RAT_ATTACK_NOTES == 0) {
                player_deal_damage(p, RAT_DAMAGE);
            }
            // If within astar range and using astar, find a path
            if(!line_of_sight && use_astar && diag_distance(p->pos, rats[i]->pos) < 20.0) {
                whitgl_ivec start = {(int)(rats[i]->pos.x), (int)(rats[i]->pos.y)};
                whitgl_ivec end = {(int)(p->pos.x), (int)(p->pos.y)};
                rats[i]->path = astar(start, end, map);
                if (rats[i]->path) {
                    astar_node_t *start_node = pop_front(&rats[i]->path);
                    free(start_node);
                }
            }
            // Follow whatever path the rat has found, if can't see player
            if(!line_of_sight && rats[i]->path) {
                astar_node_t *next_node = (astar_node_t*)rats[i]->path->data;
                whitgl_ivec target = {next_node->pt.x, next_node->pt.y};
                WHITGL_LOG("Following path -> (%f, %f)", target.x, target.y);
                rats[i]->pos = target;
                if(whitgl_ivec_eq(rats[i]->look_pos, whitgl_ivec_scale_val(rats[i]->pos, 256))) {
                    astar_node_t *reached_node = pop_front(&rats[i]->path);
                    free(reached_node);
                }
            } else if(line_of_sight) {
                // Otherwise go straight towards the player
                WHITGL_LOG("Following line of sight");
                rats[i]->path = NULL;
                whitgl_ivec target;
                target.x = p->pos.x - (p->pos.x - rats[i]->pos.x) * 0.5;
                target.y = p->pos.y - (p->pos.y - rats[i]->pos.y) * 0.5;
                rats[i]->pos = target;
            }
        }
    }
}

int get_closest_targeted_rat(whitgl_ivec player_pos, whitgl_fvec player_look, map_t *map) {
    int closest_hit_rat = -1;
    float closest_dist_sq = 0.0f;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            whitgl_fvec3 d = { rats[i]->pos.x - player_pos.x,
                rats[i]->pos.y - player_pos.y, 0.0f };
            whitgl_fvec3 l = { player_look.x, player_look.y, 0.0f };
            whitgl_fvec3 cross = whitgl_fvec3_cross(l, d);
            float sinsq = (cross.z * cross.z) / (d.x * d.x + d.y * d.y);
            float max_sinsq = RAT_HITBOX_RADIUS * RAT_HITBOX_RADIUS / (RAT_HITBOX_RADIUS * RAT_HITBOX_RADIUS + d.x*d.x + d.y*d.y);
            //WHITGL_LOG("sinsq: %f\tmax_sinsq: %f\tline_of_sight: %d", sinsq, max_sinsq, line_of_sight_exists(&player->pos, &rats[i]->pos));
            whitgl_fvec ppos = {(float)player_pos.x, (float)player_pos.y};
            whitgl_fvec rpos = {(float)rats[i]->pos.x, (float)rats[i]->pos.y};
            if (sinsq < max_sinsq && line_of_sight_exists(ppos, rpos, map)) {
                float dist_sq = d.x * d.x + d.y * d.y;
                if (dist_sq < closest_dist_sq || closest_hit_rat == -1) {
                    closest_hit_rat = i;
                    closest_dist_sq = dist_sq;
                }
            }
        }
    }
    return closest_hit_rat;
}

int get_closest_visible_rat(whitgl_ivec player_pos, map_t *map) {
    whitgl_fvec ppos = {(float)player_pos.x, (float)player_pos.y};
    int closest_visible_rat = -1;
    float closest_dist_sq = 0.0f;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            whitgl_fvec rpos = {(float)rats[i]->pos.x, (float)rats[i]->pos.y};
            whitgl_ivec d = whitgl_ivec_sub(rats[i]->pos, player_pos);
            if (line_of_sight_exists(ppos, rpos, map)) {
                float dist_sq = whitgl_ivec_sqmagnitude(d);
                if (dist_sq < closest_dist_sq || closest_visible_rat == -1) {
                    closest_visible_rat = i;
                    closest_dist_sq = dist_sq;
                }
            }
        }
    }
    return closest_visible_rat;
}

void rats_destroy_all(player_t *p) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            rat_destroy(rats[i], p);
        }
    }
}
