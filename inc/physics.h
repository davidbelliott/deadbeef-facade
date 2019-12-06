#ifndef PHYSICS_H
#define PHYSICS_H

#include "map.h"
#include <whitgl/math.h>

typedef struct physics_obj {
    unsigned int id;
    whitgl_fvec pos;
    whitgl_fvec vel;
    float mass;
    float restitution;
    float radius;
} physics_obj;

physics_obj *phys_get(int id);
void phys_update(map_t *map, float dt);
physics_obj *phys_create();
void phys_destroy(physics_obj *phys);
void set_vel(physics_obj *phys, whitgl_fvec dir, double speed);
bool move_toward_point(physics_obj *obj, whitgl_fvec *target, double speed, double thresh);

#endif // PHYSICS_H
