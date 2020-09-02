#include "graphics.h"
#include "common.h"
#include "rat.h"

#include <whitgl/sys.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>

static void draw_floor(whitgl_ivec pos, whitgl_fmat view, whitgl_fmat persp, bool ceil) {
    whitgl_fvec3 draw_pos = {pos.x + 0.5f, pos.y + 0.5f, ceil ? -1.0f : 1.0f};
    whitgl_fvec floor_offset = {64, 0};
    whitgl_fvec floor_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 2, floor_offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 3, floor_size);
    whitgl_fmat model_matrix = whitgl_fmat_translate(draw_pos);
    whitgl_sys_draw_model(ceil ? 4 : 1, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}


static void draw_floors(whitgl_fvec *player_pos, whitgl_fmat view, whitgl_fmat persp, map_t *map) {

    for (int x = (int)MAX(0.0f, player_pos->x - MAX_DIST); x < (int)MIN(MAP_WIDTH, player_pos->x + MAX_DIST); x++) {
        for (int y = (int)MAX(0.0f, player_pos->y - MAX_DIST); y < (int)MIN(MAP_HEIGHT, player_pos->y + MAX_DIST); y++) {
            if (MAP_TILE(map, x, y) == 0) {
                whitgl_ivec pos = {x, y};
                draw_floor(pos, view, persp, false);
            }
        }
    }
}


static void draw_wall(int type, whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fvec wall_offset = {tile_tex_offset[type][0], tile_tex_offset[type][1]};
    whitgl_fvec wall_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 2, wall_offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 3, wall_size);
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(0, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

void raycast(whitgl_fvec *position, double angle, whitgl_fmat view, whitgl_fmat persp, map_t *map)
{
    int n_drawn = 0;
    bool *drawn = (bool*)malloc(sizeof(bool) * map->w * map->h);
    memset(drawn, false, sizeof(bool) * map->w * map->h);
    double fov = whitgl_pi / 2 * (double)SCREEN_W/(double)SCREEN_H;
    double ray_step = fov / (double) SCREEN_W;

    for (int i = 0; i < (SCREEN_W); i++) {
        whitgl_fvec ray_direction;
        double next_x, next_y;
        double step_x, step_y;
        double max_x, max_y;
        double ray_angle;
        double dx, dy;
        int x, y;

        ray_angle = angle + -fov/2.0 + ray_step * i;
        ray_direction.x = cos(ray_angle);
        ray_direction.y = sin(ray_angle);

        x = position->x;
        y = position->y;

        step_x = _sign(ray_direction.x);
        step_y = _sign(ray_direction.y);

        next_x = x + (step_x > 0 ? 1 : 0);
        next_y = y + (step_y > 0 ? 1 : 0);

        max_x = (next_x - position->x) / ray_direction.x;
        max_y = (next_y - position->y) / ray_direction.y;

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

        bool draw = false;
        for (;;) {
            if (!tile_walkable[MAP_TILE(map, x, y)]) {
                draw = true;
                break;
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
        if (!drawn[y * map->h + x] && draw) {
            whitgl_fvec3 pos = {x + 0.5f, y + 0.5f, 0.5f};
            draw_wall(MAP_TILE(map, x, y), pos, view, persp);
            n_drawn++;
            drawn[y * map->h + x] = true;
        }
    }
    free(drawn);
    //WHITGL_LOG("n_drawn: %d", n_drawn);
}

static whitgl_fmat get_view(whitgl_fvec pov_pos, float angle) {
    //whitgl_fmat perspective = whitgl_fmat_orthographic(-5.0f, 5.0f, 5.0f, -5.0f, -5.0f, 5.0f);
    whitgl_fvec facing;
    facing.x = cos(angle);
    facing.y = sin(angle);
    whitgl_fvec3 up = {0,0,-1};
    whitgl_fvec3 camera_pos = {pov_pos.x, pov_pos.y, 0.5f};
    whitgl_fvec3 camera_to = {camera_pos.x + facing.x, camera_pos.y + facing.y,0.5f};
    whitgl_fmat view = whitgl_fmat_lookAt(camera_pos, camera_to, up);
    return view;
}

static whitgl_fmat get_persp(whitgl_float fov) {
    whitgl_fmat perspective = whitgl_fmat_perspective(fov, (float)SCREEN_W/(float)SCREEN_H, 0.1f, 100.0f);
    return perspective;
}

void draw_environment(whitgl_fvec pov_pos, float angle, map_t *map) {
    whitgl_fmat view = get_view(pov_pos, angle);
    whitgl_fmat persp = get_persp(whitgl_pi / 2.0f);
    raycast(&pov_pos, angle, view, persp, map);
    draw_floors(&pov_pos, view, persp, map);
}

void draw_entities(whitgl_fvec pov_pos, float angle) {
    whitgl_fmat view = get_view(pov_pos, angle);
    whitgl_fmat persp = get_persp(whitgl_pi / 2.0f);
    draw_rats(view, persp);
}
