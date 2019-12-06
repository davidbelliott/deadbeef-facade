#include "rat.h"
#include "common.h"
#include "game.h"
#include "astar.h"

#include <stdlib.h>
#include <math.h>

rat_t* rats[MAX_N_RATS] = {0};

rat_t* rat_get(int id) {
    return rats[id];
}

rat_t* rat_create(whitgl_fvec *pos) {
    rat_t *rat = NULL;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if(!rats[i]) {
            rat = (rat_t*)malloc(sizeof(rat_t));
            rat->id = i;
            whitgl_fvec vel = {0.0, 0.0};
            rat->phys = phys_create(pos, &vel, 0.3);
            whitgl_sprite rat_sprite = {0, {128,0}, {64,64}};
            whitgl_ivec frametr = {0, 0};
            int n_frames[MAX_N_ANIM_STATES] = {2};
            rat->anim = anim_create(rat_sprite, frametr, n_frames, NOTES_PER_MEASURE / 4, true);
            rat->path = NULL;
            rat->health = 50;
            rat->dead = false;

            for (int j = 0; j < NOTES_PER_MEASURE * MEASURES_PER_LOOP; j++) {
                note_create(&rat->beat[j], rand() % 2 == 0 && j % 16 == 0);
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
    phys_destroy(rat->phys);
    rat->phys = NULL;
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
            whitgl_fvec rat_offset = {128 + 64 * rats[i]->anim->frametr.x, 0};
            whitgl_fvec rat_size = {64, 64};
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 2, rat_offset);
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 3, rat_size);
            whitgl_fvec3 pos = {rats[i]->phys->pos.x, rats[i]->phys->pos.y, 0.5f};
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

double diag_distance(whitgl_fvec start, whitgl_fvec end) {
    double max = MAX(abs(start.x - end.x), abs(start.y - end.y));
    double min = MIN(abs(start.x - end.x), abs(start.y - end.y));
    return (max - min) + SQRT2 * min;
}

bool unobstructed(whitgl_fvec p1, whitgl_fvec p2, double w, map_t *map) {
    //if(fabs(p1->x - p2->x) < 1.0 && fabs(p1->y - p2->y) < 1.0)
    //   return true;
    whitgl_fvec v = whitgl_fvec_normalize(whitgl_fvec_sub(p2, p1));
    float dx = v.x;
    float dy = v.y;
    whitgl_fvec p11 = {p1.x - dy * w, p1.y + dx * w};
    whitgl_fvec p21 = {p2.x - dy * w, p2.y + dx * w};
    whitgl_fvec p12 = {p1.x + dy * w, p1.y - dx * w};
    whitgl_fvec p22 = {p2.x + dy * w, p2.y - dx * w};
    return line_of_sight_exists(p11, p21, map) && line_of_sight_exists(p12, p22, map);
}

void rats_update(struct player_t *p, unsigned int dt, int cur_note, bool use_astar, map_t *map) {
    for(int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            bool line_of_sight = unobstructed(rats[i]->phys->pos, p->phys->pos, rats[i]->phys->radius, map);
            // Deal damage if close enough
            if (line_of_sight && pow(rats[i]->phys->pos.x - p->phys->pos.x, 2) + pow(rats[i]->phys->pos.y - p->phys->pos.y, 2) <= pow(RAT_ATTACK_RADIUS, 2) && cur_note % RAT_ATTACK_NOTES == 0) {
                player_deal_damage(p, RAT_DAMAGE);
            }
            if(!line_of_sight && use_astar && diag_distance(p->phys->pos, rats[i]->phys->pos) < 20.0) {
                whitgl_ivec start = {(int)(rats[i]->phys->pos.x), (int)(rats[i]->phys->pos.y)};
                whitgl_ivec end = {(int)(p->phys->pos.x), (int)(p->phys->pos.y)};
                rats[i]->path = astar(start, end, map);
                if (rats[i]->path) {
                    astar_node_t *start_node = pop_front(&rats[i]->path);
                    free(start_node);
                }
            }
            if(!line_of_sight && rats[i]->path) {
                astar_node_t *next_node = (astar_node_t*)rats[i]->path->data;
                whitgl_fvec target = {(double)next_node->pt.x + 0.5, (double)next_node->pt.y + 0.5};
                bool reached = move_toward_point(rats[i]->phys, &target, RAT_SPEED, 0.1);
                if(reached) {
                    astar_node_t *reached_node = pop_front(&rats[i]->path);
                    free(reached_node);
                }
            } else if(line_of_sight) {
                rats[i]->path = NULL;
                whitgl_fvec target;
                target.x = p->phys->pos.x - (p->phys->pos.x - rats[i]->phys->pos.x) * 0.5;
                target.y = p->phys->pos.y - (p->phys->pos.y - rats[i]->phys->pos.y) * 0.5;
                move_toward_point(rats[i]->phys, &target, RAT_SPEED, 0.25);
            }
        }
    }
}

int get_closest_targeted_rat(whitgl_fvec player_pos, whitgl_fvec player_look, map_t *map) {
    int closest_hit_rat = -1;
    float closest_dist_sq = 0.0f;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            whitgl_fvec3 d = { rats[i]->phys->pos.x - player_pos.x,
                rats[i]->phys->pos.y - player_pos.y, 0.0f };
            whitgl_fvec3 l = { player_look.x, player_look.y, 0.0f };
            whitgl_fvec3 cross = whitgl_fvec3_cross(l, d);
            float sinsq = (cross.z * cross.z) / (d.x * d.x + d.y * d.y);
            float max_sinsq = RAT_HITBOX_RADIUS * RAT_HITBOX_RADIUS / (RAT_HITBOX_RADIUS * RAT_HITBOX_RADIUS + d.x*d.x + d.y*d.y);
            //WHITGL_LOG("sinsq: %f\tmax_sinsq: %f\tline_of_sight: %d", sinsq, max_sinsq, line_of_sight_exists(&player->phys->pos, &rats[i]->phys->pos));
            if (sinsq < max_sinsq && line_of_sight_exists(player_pos, rats[i]->phys->pos, map)) {
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

void rats_destroy_all(player_t *p) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (!rats[i]) {
            rat_destroy(rats[i], p);
        }
    }
}
