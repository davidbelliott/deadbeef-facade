#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "map.h"

#include <whitgl/sys.h>

void raycast(whitgl_fvec *position, double angle, whitgl_fmat view, whitgl_fmat persp, map_t *map);
void draw_environment(whitgl_fvec pov_pos, float angle, map_t *map);
void draw_entities(whitgl_fvec pov_pos, float angle);

#endif
