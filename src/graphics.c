#include "graphics.h"
#include "common.h"
#include "rat.h"
#include "key.h"
#include "game.h"
#include "music.h"

#include <whitgl/sys.h>
#include <whitgl/timer.h>
#include <whitgl/logging.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
    
#define MAX_N_EXPLOSIONS 16
#define EXPLOSION_LIFE 8
#define EXPLOSION_GROWTH_RATE 40

char notification[64];
whitgl_sys_color notif_col;
int notif_update_time;

char instruction[1024];
int instr_update_time;

typedef struct explosion_t {
    bool exists;
    whitgl_ivec pos;
    int start_note;
} explosion_t;

explosion_t explosions[MAX_N_EXPLOSIONS] = {{0}};

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
    draw_keys(view, persp);
}

void instruct(int note, const char *str) {
    strncpy(instruction, str, 1023);
    instruction[1023] = 0;
    instr_update_time = note;
}


void notify(int note, const char *str, whitgl_sys_color color) {
    strncpy(notification, str, 63);
    notification[63] = 0;
    notif_update_time = note;
    notif_col = color;
}

void draw_notif(int note, float frac_since_note) {
    if (note - notif_update_time < 8) {
        int notif_width = FONT_CHAR_W * strlen(notification);
        int top_y = SCREEN_H / 2 + (note - notif_update_time + frac_since_note) * 4;
        whitgl_iaabb notif_iaabb = {{SCREEN_W / 2 - notif_width / 2, top_y},
                     {SCREEN_W / 2 + notif_width / 2, top_y + FONT_CHAR_H}};
        draw_window(notification, notif_iaabb, notif_col);
    }
}

void clear_notif() {
    notif_update_time = -8;
}

void draw_instr(int note) {
    if (note - instr_update_time < 128) {
        int instr_width = SCREEN_W / 2;
        int top_y = 2 * FONT_CHAR_H;
        whitgl_iaabb instr_iaabb = {{SCREEN_W / 2 - instr_width / 2, top_y},
            {SCREEN_W / 2 + instr_width / 2, top_y + 4 * FONT_CHAR_H}};
        whitgl_iaabb text_iaabb = {{instr_iaabb.a.x, instr_iaabb.a.y + FONT_CHAR_H},
            instr_iaabb.b};

        char wrapped[1024];
        wrap_text(instruction, wrapped, 1024, text_iaabb);
        whitgl_sys_color border = {0, 0, 128, 255};
        whitgl_sys_color fill = {0, 0, 0, 255};
        whitgl_sys_draw_iaabb(instr_iaabb, border);
        whitgl_ivec title_pos = {text_iaabb.a.x, instr_iaabb.a.y};
        whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
        whitgl_sys_draw_text(font, "INSTR:data/lvl/lvl1.json", title_pos);
        whitgl_sys_draw_iaabb(text_iaabb, fill);
        draw_str_with_newlines(wrapped, 1024, text_iaabb.a);
    }
}

void draw_top_bar(int note, player_t *player) {
    int most_recent_note = note % 8;
    whitgl_sys_color fill_a = {0, 0, 128, 255};
    whitgl_sys_color fill_b = {128, 0, 0, 255};
    whitgl_sys_color top_col = most_recent_note < 2 ? fill_b : fill_a;
    whitgl_iaabb top_iaabb = {{0, 0}, {SCREEN_W, FONT_CHAR_H}};
    char key_str[4] = "   ";
    if (player->keys & KEY_R)
        key_str[0] = 'R';
    if (player->keys & KEY_G)
        key_str[1] = 'G';
    if (player->keys & KEY_B)
        key_str[2] = 'B';
    char path[256];
    snprintf(path, 256, "[PATH: data/lvl/lvl1] [KEYS: %s] [FPS: %d]", key_str, whitgl_timer_fps());
    draw_window(path, top_iaabb, top_col);
}

void draw_health_bar(int note, player_t *player) {
    int elapsed = note - player->last_damage;
    if (elapsed < 0) {
        elapsed += music_get_song_len();
    }
    int blue_val = (elapsed < 8 ? 255.0 / (8.0 - elapsed) : 255);
    int red_val = 255 - blue_val;
    whitgl_sys_color col = {red_val, 0, blue_val, 255};
    whitgl_iaabb bottom_iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W, SCREEN_H}};
    whitgl_iaabb health_iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W * player->health / 100, SCREEN_H}};
    whitgl_sys_color black = {0, 0, 0, 255};
    char health[256];
    snprintf(health, 256, "[VIT: %.2f]", player->health / 100.0);
    whitgl_sys_draw_iaabb(bottom_iaabb, black);
    draw_window(health, health_iaabb, col);
}

void add_explosion(int note, whitgl_ivec screen_pos) {
    int idx;
    for (idx = 0; idx < MAX_N_EXPLOSIONS; idx++) {
        if (!explosions[idx].exists) {
            break;
        }
    }
    if (idx == MAX_N_EXPLOSIONS) {
        return;
    }
    explosions[idx].exists = true;
    explosions[idx].pos = screen_pos;
    explosions[idx].start_note = note;
}

void draw_explosions(int note, float frac_since_note) {
    for (int idx = 0; idx < MAX_N_EXPLOSIONS; idx++) {
        if (explosions[idx].exists) {
            if (explosions[idx].start_note > note ||
                note - explosions[idx].start_note > EXPLOSION_LIFE) {
                explosions[idx].exists = false;
            } else {
                whitgl_ivec pos = explosions[idx].pos;
                int hsize = (note - explosions[idx].start_note + frac_since_note) * \
                            EXPLOSION_GROWTH_RATE;
                whitgl_iaabb box = {{pos.x - hsize, pos.y - hsize}, \
                    {pos.x + hsize, pos.y + hsize}};
                whitgl_sys_color white = {255, 255, 255, 255};
                whitgl_sys_draw_hollow_iaabb(box, 1, white);
            }
        }
    }
}

void draw_billboard(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(2, WHITGL_SHADER_EXTRA_1, model_matrix, view, persp);
}
