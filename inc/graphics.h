#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "map.h"

#include <whitgl/sys.h>

struct player_t;

void raycast(whitgl_fvec *position, double angle, whitgl_fmat view, whitgl_fmat persp, map_t *map);
void draw_environment(whitgl_fvec pov_pos, float angle, map_t *map);
void draw_entities(whitgl_fvec pov_pos, float angle);
void notify(int note, const char *str, whitgl_sys_color color);
void draw_notif(int note);
void clear_notif();

void instruct(int note, const char *str);
void draw_instr(int note);

void draw_top_bar(int note);
void draw_health_bar(int note, struct player_t *player);

#endif
