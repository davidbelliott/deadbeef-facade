#include "physics.h"
#include "common.h"

#include <stddef.h>
#include <stdlib.h>

#include <whitgl/math.h>

physics_obj* phys_objs[MAX_N_PHYS] = {0};


static whitgl_fvec clip_move(physics_obj *phys, whitgl_fvec new_pos, map_t *map);

physics_obj *phys_get(int id) {
    return phys_objs[id];
}

static bool obj_vs_obj(physics_obj *a, physics_obj *b, map_t *map) {
    whitgl_fvec n = whitgl_fvec_sub(a->pos, b->pos);
    float r = a->radius + b->radius;
    r *= r;
    float n_sq = whitgl_fvec_sqmagnitude(n);
    if (n_sq > r) {
        return false;
    }

    float d = whitgl_fsqrt(n_sq);
    float penetration = a->radius + b->radius - d;
    whitgl_fvec normal = whitgl_fvec_normalize(n);
    a->pos = clip_move(a, whitgl_fvec_add(a->pos, whitgl_fvec_scale_val(normal, penetration * 0.5f)), map);
    b->pos = clip_move(b, whitgl_fvec_sub(b->pos, whitgl_fvec_scale_val(normal, penetration * 0.5f)), map);
    return true;
}

physics_obj* phys_create(whitgl_fvec *pos, whitgl_fvec *vel, double radius) {
    physics_obj *ptr = NULL;
    for (int i = 0; i < MAX_N_PHYS; i++) {
        if (!phys_objs[i]) {
            ptr = (physics_obj*)malloc(sizeof(physics_obj));
            ptr->id = i;
            ptr->pos = *pos;
            ptr->vel = *vel;
            ptr->radius = radius;
            ptr->restitution = 0.7f;
            ptr->mass = 1.0f;
            phys_objs[ptr->id] = ptr;
            break;
        }
    }
    return ptr;
}

void phys_destroy(physics_obj *phys) {
    phys_objs[phys->id] = NULL;
    free(phys);
}

void set_vel(physics_obj *phys, whitgl_fvec dir, double speed) {
    phys->vel = whitgl_fvec_scale_val(whitgl_fvec_normalize(dir), speed);
}

// See whether it's possible to move to a position, or if a wall blocks it
static bool try_to_move(physics_obj *obj, map_t *map, whitgl_fvec pos)
{
    int x0, x1, y0, y1;
    float size = obj->radius;

    x0 = pos.x - size;
    x1 = pos.x + size;
    y0 = pos.y - size;
    y1 = pos.y + size;

    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            if (!tile_walkable[MAP_GET(map, x, y)])
                return false;
        }
    }
    return true;
}

// Clip movement based on presence of walls
static whitgl_fvec clip_move(physics_obj *phys, whitgl_fvec new_pos, map_t *map) {
    whitgl_fvec pos;
    whitgl_fvec old_pos = phys->pos;
    whitgl_fvec actual_pos = old_pos;

    pos.x = new_pos.x;
    pos.y = new_pos.y;
    if(try_to_move(phys, map, pos)) {
        return pos;
    }

    pos.x = new_pos.x;
    pos.y = old_pos.y;
    if(try_to_move(phys, map, pos)) {
        return pos;
    }

    pos.x = old_pos.x;
    pos.y = new_pos.y;
    if(try_to_move(phys, map, pos)) {
        return pos;
    }

    return old_pos;
}

// Move a physics object, resolving collisions with walls and other objects
// if possible
static void move(physics_obj *phys, whitgl_fvec new_pos, map_t *map) {
    whitgl_fvec pos;
    whitgl_fvec old_pos = phys->pos;

    pos = clip_move(phys, new_pos, map);
    phys->pos = pos;
    for (int i = 0; i < MAX_N_PHYS_PASSES; i++) {
        bool no_collision = true;
        for (int i = 0; i < MAX_N_PHYS; i++) {
            if (phys_objs[i] && i != phys->id) {
                if (obj_vs_obj(phys, phys_objs[i], map)) {
                    no_collision = false;
                }
            }
        }
        if (no_collision)
            break;
    }
}

static void phys_obj_update(physics_obj *phys, map_t *map, float dt) {
    //printf("Moving with vel (%f, %f)\n", phys->vel.x, phys->vel.y);
    whitgl_fvec new_pos;
    new_pos.x = phys->pos.x + phys->vel.x * dt;
    new_pos.y = phys->pos.y + phys->vel.y * dt;
    move(phys, new_pos, map);
}

void phys_update(map_t *map, float dt) {
    for(int i = 0; i < MAX_N_PHYS; i++) {
        if (phys_objs[i]) {
            phys_obj_update(phys_objs[i], map, dt);
        }
    }
}

bool move_toward_point(physics_obj *obj, whitgl_fvec *target, double speed, double thresh) {
    if(whitgl_fabs(target->x - obj->pos.x) <= thresh && whitgl_fabs(target->y - obj->pos.y) <= thresh) {
        obj->vel.x = 0.0;
        obj->vel.y = 0.0;
        return true;
    }
    whitgl_fvec dir = whitgl_fvec_sub(*target, obj->pos);
    set_vel(obj, dir, speed);
    return false;
}

