#include "game.h"
#include "common.h"
#include "map.h"
#include "anim.h"
#include "rat.h"
#include "physics.h"
#include "intro.h"

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char notification[64];
whitgl_sys_color notif_col;
int notif_update_time;

char instruction[1024] = "";
int instr_update_time;

float note_calib_offset = 0.00f;
int lvl_grace_period = 32;

#define MOVE_NONE       0
#define MOVE_FORWARD    1
#define MOVE_BACKWARD   2
#define MOVE_TURN_LEFT  3
#define MOVE_TURN_RIGHT 4

whitgl_ivec crosshair_pos = {SCREEN_W / 2 - 8, SCREEN_H / 2 - 8};

typedef struct note_pop_text_t {
    bool exists;
    float life;
    char text[SHORT_TEXT_MAX_LEN];
} note_pop_text_t;

enum {
    RAT_TYPE_BASIC,
    RAT_TYPE_SPRINTING,
    RAT_TYPE_TURRET,
    RAT_TYPE_REPRODUCER
};

typedef struct loop_t {
    char name[SHORT_TEXT_MAX_LEN];
    note_t notes[TOTAL_NUM_NOTES];
} loop_t;

int earliest_active_note_offset = 0;
int note = 0;

note_pop_text_t note_pop_text = { 0 };


player_t* player = NULL;

int level = 1;
map_t map = {0};

int cycles_to_astar = 0;
int actual_song_millis = 0;
int prev_actual_song_millis = 0;

whitgl_float time_since_note = 0;
whitgl_float song_time = 0.0f;
whitgl_float song_len = 0.0f;

// Definitions of static helper functions

static int get_targeted_rat(int cur_targeted, whitgl_ivec player_pos, whitgl_ivec player_look_pos) {
    whitgl_fvec player_look = {(float)player_look_pos.x / 256.0f + 0.5f, (float)player_look_pos.y / 256.0f + 0.5f};
    int next_targeted = get_closest_targeted_rat(player_pos, player_look, &map);
    return next_targeted;
}

static void player_destroy() {
    free(player);
    player = NULL;
}

static void player_create(whitgl_ivec pos, unsigned int angle) {
    if(player)
        player_destroy();
    player = (player_t*)malloc(sizeof(player_t));
    whitgl_fvec zero = {0.0, 0.0};
    whitgl_ivec izero = {0, 0};
    player->pos = pos;
    player->look_pos.x = pos.x * 256;
    player->look_pos.y = pos.y * 256;
    player->angle = angle;
    player->look_angle = angle;
    player->facing = izero;
    player->look_facing = zero;
    player->move = 0;
    player->last_rotate_dir = 1;
    player->health = 100;
    player->last_damage = -256;
    player->targeted_rat = -1;
    player->moved = false;
}

void game_load_level(char *levelname, map_t *map) {
    unsigned char *data;
    whitgl_int w = 0, h = 0;
    bool ret = whitgl_sys_load_png(levelname, &w, &h, &data);
    if (!ret) {
        map->tiles = NULL;
        return;
    }

    map->w = (int)w;
    map->h = (int)h;
    map->tiles = (unsigned char*)malloc(sizeof(unsigned char) * w * h);
    map->entities = (unsigned char*)malloc(sizeof(unsigned char) * w * h);
    memset(map->tiles, 0, sizeof(unsigned char) * w * h);

    int data_idx, tile;
    for (int i = 0; i < w * h; i++) {
        data_idx = 4 * i;
        tile = 0;
        for (int j = 0; j < N_TILE_TYPES; j++) {
            if (tile_lvl_rgb[j][0] == data[data_idx]
                    && tile_lvl_rgb[j][1] == data[data_idx + 1]
                    && tile_lvl_rgb[j][2] == data[data_idx + 2]) {
                tile = j;
                break;
            }
        }
        map->tiles[i] = tile;
        map->entities[i] = ENTITY_TYPE_NONE;
    }
}

void notify(const char *str, whitgl_sys_color color) {
    strncpy(notification, str, 64);
    notification[64] = 0;
    notif_update_time = note;
    notif_col = color;
}

void instruct(const char *str) {
    strncpy(instruction, str, 1024);
    notification[1024] = 0;
    instr_update_time = note;
}

void game_free_level(map_t *map) {
    free(map->tiles);
    free(map->entities);
}

void set_note_pop_text(char text[SHORT_TEXT_MAX_LEN]) {
    note_pop_text.exists = true;
    note_pop_text.life = 60.0f / BPM;
    strncpy(note_pop_text.text, text, SHORT_TEXT_MAX_LEN);
}

static double degrees_to_radians(double angle) {
	return angle * M_PI/180.0;
}

whitgl_fmat make_billboard_mat(whitgl_fmat BM, whitgl_fmat const MV)
{
    whitgl_fmat ret = BM;
    for(size_t i = 0; i < 3; i++) {
        for(size_t j = 0; j < 3; j++) {
            ret.mat[i*4+j] = i==j ? 1 : 0;
        }
        ret.mat[i*4+3] = MV.mat[i*4+3];
    }

    for(size_t i = 0; i < 4; i++) {
        ret.mat[3*4+i] = MV.mat[3*4+i];
    }
    return ret;
}

/*void print_list(node_t **head) {
    node_t *s = *head;
    while(s) {
        printf("(%d, %d)", s->pt.x, s->pt.y);
        s = s->next;
        if(s)
            printf("->");
    }
    printf("\n");
}

void print_path(node_t *end) {
    node_t *from = end;
    while(from) {
        printf("(%d, %d)<-", from->pt.x, from->pt.y);
        from = from->from;
    }
    printf("\n");
}*/


static void draw_skybox(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(3, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

static void draw_wall(int type, whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fvec wall_offset = {tile_tex_offset[type][0], tile_tex_offset[type][1]};
    whitgl_fvec wall_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 2, wall_offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 3, wall_size);
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(0, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

static void draw_floor(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fvec floor_offset = {64, 0};
    whitgl_fvec floor_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 2, floor_offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 3, floor_size);
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(1, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

static void draw_note_overlay() {
    if (note < lvl_grace_period - 8)
        return;
    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    float time_to_next_beat = (note / 8 + 1) * 8 * secs_per_note - song_time + 0.01f;
    float frac_to_next_beat = time_to_next_beat / secs_per_note / 8.0f;

    int width = SCREEN_W * frac_to_next_beat / 1;
    //int height = (SCREEN_H - 2 * FONT_CHAR_H) * frac_to_next_beat;
    int height = SCREEN_W * frac_to_next_beat / 1;

    whitgl_sys_color white = {255, 255, 255, 255};
    whitgl_iaabb iaabb = {{SCREEN_W / 2 - width / 2, SCREEN_H / 2 - height / 2},
        {SCREEN_W / 2 + width / 2, SCREEN_H / 2 + height / 2}};
    whitgl_sys_draw_hollow_iaabb(iaabb, 2, white);
}

static void draw_overlay(int cur_note) {

    draw_note_overlay();

    // Draw top bar
    int most_recent_note = note % 8;
    whitgl_sys_color fill_a = {0, 0, 128, 255};
    whitgl_sys_color fill_b = {128, 0, 0, 255};
    whitgl_sys_color top_col = most_recent_note < 2 ? fill_b : fill_a;
    whitgl_iaabb top_iaabb = {{0, 0}, {SCREEN_W, FONT_CHAR_H}};
    char path[256] = "[PATH: data/lvl/lvl1] [KEYS: RGB] [FPS: 60]";
    draw_window(path, top_iaabb, top_col);

    // Draw health bar
    int elapsed = note - player->last_damage;
    int blue_val = (elapsed < 8 ? 255.0 / (8.0 - elapsed) : 255);
    int red_val = 255 - blue_val;
    whitgl_sys_color col = {red_val, 0, blue_val, 255};
    whitgl_iaabb bottom_iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W, SCREEN_H}};
    whitgl_iaabb health_iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W * player->health / 100, SCREEN_H}};
    whitgl_sys_color black = {0, 0, 0, 255};
    char health[256];
    snprintf(health, 256, "HEALTH: %d", player->health);
    whitgl_sys_draw_iaabb(bottom_iaabb, black);
    draw_window(health, health_iaabb, col);

    if (note - notif_update_time < 8) {
        int notif_width = FONT_CHAR_W * strlen(notification);
        int top_y = SCREEN_H / 2 + (note - notif_update_time) * FONT_CHAR_H;
        whitgl_iaabb notif_iaabb = {{SCREEN_W / 2 - notif_width / 2, top_y},
                     {SCREEN_W / 2 + notif_width / 2, top_y + FONT_CHAR_H}};
        draw_window(notification, notif_iaabb, notif_col);
    }

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

static whitgl_ivec point_project(whitgl_fvec3 pos, whitgl_fmat mv, whitgl_fmat projection) {
    float objx = pos.x;
    float objy = pos.y;
    float objz = pos.z;
    // Transformation vectors
    float fTempo[8];

    whitgl_ivec window_coord = {0, 0};
    // Modelview transform
    fTempo[0]=mv.mat[0]*objx+mv.mat[4]*objy+mv.mat[8]*objz+mv.mat[12]; // w is always 1
    fTempo[1]=mv.mat[1]*objx+mv.mat[5]*objy+mv.mat[9]*objz+mv.mat[13];
    fTempo[2]=mv.mat[2]*objx+mv.mat[6]*objy+mv.mat[10]*objz+mv.mat[14];
    fTempo[3]=mv.mat[3]*objx+mv.mat[7]*objy+mv.mat[11]*objz+mv.mat[15];
    // Projection transform, the final row of projection matrix is always [0 0 -1 0]
    // so we optimize for that.
    fTempo[4]=projection.mat[0]*fTempo[0]+projection.mat[4]*fTempo[1]+projection.mat[8]*fTempo[2]+projection.mat[12]*fTempo[3];
    fTempo[5]=projection.mat[1]*fTempo[0]+projection.mat[5]*fTempo[1]+projection.mat[9]*fTempo[2]+projection.mat[13]*fTempo[3];
    fTempo[6]=projection.mat[2]*fTempo[0]+projection.mat[6]*fTempo[1]+projection.mat[10]*fTempo[2]+projection.mat[14]*fTempo[3];
    fTempo[7]=-fTempo[2];
    // The result normalizes between -1 and 1
    if(fTempo[7]==0.0) // The w value
        return window_coord;
    fTempo[7]=1.0/fTempo[7];
    // Perspective division
    fTempo[4]*=fTempo[7];
    fTempo[5]*=fTempo[7];
    fTempo[6]*=fTempo[7];
    // Window coordinates
    // Map x, y to range 0-1
    window_coord.x=(fTempo[4]*0.5+0.5)*SCREEN_W;
    window_coord.y=(fTempo[5]*0.5+0.5)*SCREEN_H;
    // This is only correct when glDepthRange(0.0, 1.0)
    //windowCoordinate[2]=(1.0+fTempo[6])*0.5;	// Between 0 and 1
    WHITGL_LOG("window coord: %d, %d", window_coord.x, window_coord.y);
    return window_coord;
}

static void draw_note_pop_text(whitgl_ivec pos) {
    if (note_pop_text.exists) {
        int str_len = strlen("BASED");
        int x = pos.x - str_len * FONT_CHAR_W;
        int y = pos.y + 8 + 20.0f * (note_pop_text.life);
        whitgl_iaabb iaabb = {{x, y - FONT_CHAR_H}, {x + str_len * FONT_CHAR_W, y}};
        whitgl_sys_color col = {0, 0, 0, 255};
        whitgl_sys_draw_iaabb(iaabb, col);

        whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
        whitgl_ivec text_pos = { x, y - FONT_CHAR_H};
        whitgl_sys_draw_text(font, note_pop_text.text, text_pos);
    }
}


static void draw_rat_overlays(int targeted_rat, int cur_note, float time_since_note, whitgl_fmat view, whitgl_fmat persp) {
    if (targeted_rat != -1) {
        rat_t *target = rat_get(targeted_rat);
        note_t *beat = target->beat;
        whitgl_fvec3 model_pos = {0.0f, 0.0f, 0.0f};
        whitgl_fvec3 pos = {(float)target->pos.x, (float)target->pos.y, 0.5f};
        whitgl_fmat mv = whitgl_fmat_multiply(view, whitgl_fmat_translate(pos));
        whitgl_ivec window_coords = point_project(model_pos, mv, persp);
        /*if (whitgl_ivec_dot(player->look_direction, whitgl_ivec_sub(target->pos, player->pos)) < 0) {
            window_coords.x = window_coords.x >= SCREEN_W / 2 ? 8 : SCREEN_W - 8;
        }*/
        if (window_coords.x < 8) {
            window_coords.x = 8;
        } else if (window_coords.x > SCREEN_W - 8) {
            window_coords.x = SCREEN_W - 8;
        }

        whitgl_sys_color fill = {0, 0, 0, 255};
        whitgl_sys_color border = {255, 255, 255, 255};
        int pixel_x = window_coords.x;
        whitgl_iaabb zune_iaabb = {{pixel_x - 8, 0}, {pixel_x + 8, SCREEN_H / 2 + 8}};
        whitgl_iaabb line1 = {{pixel_x - 7, 0}, {pixel_x - 7, SCREEN_H / 2 - 8}};
        whitgl_iaabb line2 = {{pixel_x + 8, 0}, {pixel_x + 8, SCREEN_H / 2 - 8}};
        whitgl_iaabb line3 = {{pixel_x + 8, SCREEN_H / 2 - 8}, {pixel_x, SCREEN_H / 2}};
        whitgl_iaabb line4 = {{pixel_x - 7, SCREEN_H / 2 - 7}, {pixel_x, SCREEN_H / 2}};
        whitgl_sys_draw_iaabb(zune_iaabb, fill);
        whitgl_sys_draw_line(line1, border);
        whitgl_sys_draw_line(line2, border);
        //whitgl_sys_draw_line(line3, border);
        //whitgl_sys_draw_line(line4, border);
        draw_notes(beat, cur_note, time_since_note, pixel_x - 8);

        whitgl_ivec target_pos = crosshair_pos;
        target_pos.x = pixel_x - 8;
        whitgl_sprite sprite = {0, {128,64+16*2},{16,16}};
        whitgl_ivec frametr = {0, 0};
        whitgl_sys_draw_sprite(sprite, frametr, target_pos);

        draw_note_pop_text(target_pos);
    }
    whitgl_ivec crosshairs_pos = {SCREEN_W / 2, SCREEN_H / 2};
    whitgl_iaabb box1 = {{crosshairs_pos.x - 1, crosshairs_pos.y - 4}, {crosshairs_pos.x + 1, crosshairs_pos.y + 4}};
    whitgl_iaabb box2 = {{crosshairs_pos.x - 4, crosshairs_pos.y - 1}, {crosshairs_pos.x + 4, crosshairs_pos.y + 1}};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_sys_draw_iaabb(box1, border);
    whitgl_sys_draw_iaabb(box2, border);
}

static void draw_sky() {
}

static void draw_floors(whitgl_fvec *player_pos, whitgl_fmat view, whitgl_fmat persp) {

    for (int x = (int)MAX(0.0f, player_pos->x - MAX_DIST); x < (int)MIN(MAP_WIDTH, player_pos->x + MAX_DIST); x++) {
        for (int y = (int)MAX(0.0f, player_pos->y - MAX_DIST); y < (int)MIN(MAP_HEIGHT, player_pos->y + MAX_DIST); y++) {
            if (MAP_TILE(&map, x, y) == 0) {
                whitgl_fvec3 pos = {x + 0.5f, y + 0.5f, 1.0f};
                draw_floor(pos, view, persp);
            }
        }
    }
}

static void raycast(whitgl_fvec *position, double angle, whitgl_fmat view, whitgl_fmat persp)
{
    int n_drawn = 0;
    bool *drawn = (bool*)malloc(sizeof(bool) * map.w * map.h);
    memset(drawn, false, sizeof(bool) * map.w * map.h);
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
            if (!tile_walkable[MAP_TILE(&map, x, y)]) {
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
        if (!drawn[y * map.h + x] && draw) {
            whitgl_fvec3 pos = {x + 0.5f, y + 0.5f, 0.5f};
            draw_wall(MAP_TILE(&map, x, y), pos, view, persp);
            n_drawn++;
            drawn[y * map.h + x] = true;
        }
    }
    free(drawn);
    //WHITGL_LOG("n_drawn: %d", n_drawn);
}


static void frame(player_t *p, int cur_note)
{
    whitgl_fvec pov_pos = {(float)player->look_pos.x / 256.0f + 0.5f, (float)player->look_pos.y / 256.0f + 0.5f};
    whitgl_sys_draw_init(0);

    whitgl_sys_enable_depth(false);

    draw_sky();

    whitgl_sys_enable_depth(true);

    whitgl_float fov = whitgl_pi/2;
    whitgl_fmat perspective = whitgl_fmat_perspective(fov, (float)SCREEN_W/(float)SCREEN_H, 0.1f, 100.0f);
    //whitgl_fmat perspective = whitgl_fmat_orthographic(-5.0f, 5.0f, 5.0f, -5.0f, -5.0f, 5.0f);
    whitgl_fvec3 up = {0,0,-1};
    whitgl_fvec3 camera_pos = {pov_pos.x, pov_pos.y, 0.5f};
    whitgl_fvec3 camera_to = {camera_pos.x + player->look_facing.x, camera_pos.y + player->look_facing.y,0.5f};
    whitgl_fmat view = whitgl_fmat_lookAt(camera_pos, camera_to, up);
    //whitgl_fmat model_matrix = whitgl_fmat_translate(camera_to);
    //model_matrix = whitgl_fmat_multiply(model_matrix, whitgl_fmat_rot_z(time*3));

    //whitgl_sys_draw_model(0, WHITGL_SHADER_MODEL, model_matrix, view, perspective);
    whitgl_fvec3 skybox_pos = camera_pos;
    skybox_pos.z = -2.5f;
    //draw_skybox(skybox_pos, view, perspective);
    raycast(&pov_pos, p->look_angle * whitgl_pi / 128.0f, view, perspective);
    draw_floors(&pov_pos, view, perspective);
    draw_rats(view, perspective);

    whitgl_sys_enable_depth(false);

    draw_rat_overlays(player->targeted_rat, cur_note, time_since_note, view, perspective);

    //draw_notes(cur_loop, cur_note, time_since_note);
    draw_overlay(cur_note);

    // draw move overlay if needed
    int elapsed = note - player->move_time;
    int color_amt = (elapsed < 4 ? (4 - elapsed) * 32 : 0);
    whitgl_sys_color c = {0, 0, 0, 0};
    switch (p->move_goodness) {
        case 2:
            c.g = color_amt;
            break;
        case 1:
            c.g = color_amt;
        case 0:
            c.r = color_amt;
        default:
            break;
    }
    whitgl_set_shader_color(WHITGL_SHADER_EXTRA_0, 4, c);

    whitgl_sys_draw_finish();
}


void player_deal_damage(player_t *p, int dmg) {
    p->last_damage = note;
    p->health = MAX(0, p->health - dmg);
}

static void player_on_note(player_t *p, int note) {
    if (note % 8 == 4) {
        if (note >= lvl_grace_period) {
            if (!p->moved) {
                whitgl_sys_color col = {128, 0, 0, 255};
                notify("YOU DIDN'T MOVE!", col);
                player_deal_damage(p, 2);
            }
            p->moved = false;
        }
    } else if (note % 8 == 0 && note < lvl_grace_period) {
        int beats_left = (lvl_grace_period - note) / 8;
        char str[64];
        const char *template = "%d...";
        if (beats_left > 3)
            snprintf(str, 64, template, beats_left);
        else if (beats_left == 3)
            strncpy(str, "READY", 64);
        else if (beats_left == 2)
            strncpy(str, "SET", 64);
        else
            strncpy(str, "GO!", 64);
        whitgl_sys_color black = {0, 0, 0, 255};
        notify(str, black);
    }
}

static int player_update(player_t *p, int note)
{
    if (p->angle != p->look_angle && p->last_rotate_dir == 1) {
        p->look_angle = (p->look_angle + 8) % 256;
    } else if (p->angle != p->look_angle) {
        p->look_angle = (p->look_angle - 8) % 256;
    }

    if (p->pos.x * 256 > p->look_pos.x) {
        p->look_pos.x += 32;
    } else if (p->pos.x * 256 < p->look_pos.x) {
        p->look_pos.x -= 32;
    }
    if (p->pos.y * 256 > p->look_pos.y) {
        p->look_pos.y += 32;
    } else if (p->pos.y * 256 < p->look_pos.y) {
        p->look_pos.y -= 32;
    }

    p->look_facing.x = cos(p->look_angle * whitgl_pi / 128.0f);
    p->look_facing.y = sin(p->look_angle * whitgl_pi / 128.0f);
    p->facing.x = cos(p->angle * whitgl_pi / 128.0f);
    p->facing.y = sin(p->angle * whitgl_pi / 128.0f);
    
    // Target if no rat targeted
    if (p->targeted_rat == -1) {
        p->targeted_rat = get_closest_visible_rat(p->pos, &map);
    }

    // Test for portals, doors, etc
    if (MAP_TILE(&map, p->pos.x, p->pos.y) == TILE_TYPE_PORTAL) {
        level++;
        char levelstr[256];
        snprintf(levelstr, 256, "data/lvl/lvl%d.png", level);
        intro_set_text(levelstr);
        game_free_level(&map);
        game_load_level(levelstr, &map);
        return GAME_STATE_INTRO;
    }
    return GAME_STATE_GAME;
}

/*static void notes_update(struct player_t *p, int cur_loop, int cur_note) {
    if (loop_active) {
        earliest_active_note_offset = MAX(-NOTES_PER_MEASURE / 8, earliest_active_note_offset - 1);
        int bottom_note_idx = _mod(cur_note - NOTES_PER_MEASURE / 4 - 1, NOTES_PER_MEASURE * MEASURES_PER_LOOP);
        note_t *bottom_note = &aloops[cur_loop].notes[bottom_note_idx];
        if (bottom_note->exists) {
            if (bottom_note->popped) {
                bottom_note->popped = false;
            }
            bottom_note->anim->frametr.x = 0;
            bottom_note->anim->frametr.y = 0;
        }
    }
}*/

static void note_pop_text_update(float dt) {
        if (note_pop_text.exists) {
            note_pop_text.life = MAX(0.0f, note_pop_text.life - dt);
            if (note_pop_text.life == 0.0f) {
                note_pop_text.exists = false;
            }
        }
}


void game_pause(bool paused) {
    whitgl_loop_set_paused(AMBIENT_MUSIC, paused);
}

void game_input()
{
    player_t *p = player;

    p->move = MOVE_NONE;
    if(whitgl_input_pressed(WHITGL_INPUT_UP))
        p->move = MOVE_FORWARD;
    if(whitgl_input_pressed(WHITGL_INPUT_DOWN))
        p->move = MOVE_BACKWARD;
    /*if(whitgl_input_held(WHITGL_INPUT_RIGHT))
        move_dir.y -= 1.0;
    if(whitgl_input_held(WHITGL_INPUT_LEFT))
        move_dir.y += 1.0;*/
    if (whitgl_input_pressed(WHITGL_INPUT_RIGHT)) {
        p->move = MOVE_TURN_RIGHT;
    }
    if (whitgl_input_pressed(WHITGL_INPUT_LEFT)) {
        p->move = MOVE_TURN_LEFT;
    }
    
    if (p->move != MOVE_NONE && note >= lvl_grace_period) {

        whitgl_ivec newpos = p->pos;
        if (p->move == MOVE_FORWARD) {
            newpos = whitgl_ivec_add(p->pos, p->facing);
        } else if (p->move == MOVE_BACKWARD) {
            newpos = whitgl_ivec_sub(p->pos, p->facing);
        } else if (p->move == MOVE_TURN_LEFT) {
            p->angle = (p->angle - 64) % 256;
            p->last_rotate_dir = -1;
        } else if (p->move == MOVE_TURN_RIGHT) {
            p->angle = (p->angle + 64) % 256;
            p->last_rotate_dir = +1;
        }


        p->moved = true;
        p->move_time = note;

        float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
        float time_to_next_beat = (note + 8) * secs_per_note - song_time + note_calib_offset;
        float time_since_prev_beat = song_time - note_calib_offset - (int)(note / 8) * 8 * secs_per_note;
        float best_time = whitgl_fmin(time_to_next_beat, time_since_prev_beat);
        printf("%f\n", best_time);
        whitgl_sys_color green = {0, 128, 0, 255};
        whitgl_sys_color yellow = {128, 128, 0, 255};
        whitgl_sys_color red = {128, 0, 0, 255};
        whitgl_sys_color black = {0, 0, 0, 255};
        if (best_time < 0.10f) {
            notify("GOOD", black);
            p->move_goodness = 2;
            p->health += 1;
        } else if (best_time < 0.20f) {
            notify("MEDIOCRE", black);
            p->move_goodness = 1;
        } else {
            notify("BAD", black);
            player_deal_damage(p, 2);
            p->move_goodness = 0;
        }

        if (p->move == MOVE_FORWARD || p->move == MOVE_BACKWARD) {
            if (tile_walkable[MAP_TILE(&map, newpos.x, newpos.y)]
                    && !MAP_ENTITY(&map, newpos.x, newpos.y)) {
                MAP_SET_ENTITY(&map, p->pos.x, p->pos.y, ENTITY_TYPE_NONE);
                p->pos = newpos;
                MAP_SET_ENTITY(&map, newpos.x, newpos.y, ENTITY_TYPE_PLAYER);
            } else {
                whitgl_sys_color red = {128, 0, 0, 255};
                notify("You crashed!", red);
                player_deal_damage(p, 5);
            }
        }
    }


    if (whitgl_input_pressed(WHITGL_INPUT_A)) {
        /*int targeted = get_targeted_rat(player->targeted_rat, player->pos, player->look_direction);
        if (targeted != -1) {
            player->targeted_rat = targeted;
        }*/
    }
    
    if (whitgl_input_pressed(WHITGL_INPUT_MOUSE_LEFT)) {
        if (p->targeted_rat != -1) {
            int best_candidate = NOTES_PER_MEASURE/8+1;
            int best_idx = -1;
            note_t *beat = rat_get(p->targeted_rat)->beat;
            for (int i = -NOTES_PER_MEASURE/8; i < NOTES_PER_MEASURE/8; i++) {
                int note_idx = _mod(note + i, NOTES_PER_MEASURE * MEASURES_PER_LOOP);
                if (beat[note_idx].exists && !beat[note_idx].popped && abs(i) < best_candidate) {
                    best_idx = note_idx;
                    best_candidate = abs(i);
                }
            }
            if (best_idx != -1) {
                float acc = 1.0f - (float)best_candidate / (float)(NOTES_PER_MEASURE / 8);
                //beat[best_idx].popped = true;
                //beat[best_idx].anim->frametr.x = 0;
                //beat[best_idx].anim->frametr.y = 1;
                if (acc <= 0.25f) {
                    set_note_pop_text("BAD");
                } else if (acc <= 0.50f) {
                    set_note_pop_text("OK");
                } else if (acc <= 0.75f) {
                    set_note_pop_text("GOOD");
                } else {
                    set_note_pop_text("BASED");
                }
                rat_deal_damage(rat_get(p->targeted_rat), (int)(4.0f * acc));
            }
        }
    }

    whitgl_ivec mouse_pos = whitgl_input_mouse_pos(PIXEL_DIM);
    //p->angle = mouse_pos.x * MOUSE_SENSITIVITY / (double)(SCREEN_W);
}

void game_init() {
    whitgl_load_model(0, "data/obj/cube.wmd");
    whitgl_load_model(1, "data/obj/floor.wmd");
    whitgl_load_model(2, "data/obj/billboard.wmd");
    whitgl_load_model(3, "data/obj/skybox.wmd");

    level = 1;
    game_load_level("data/lvl/lvl1.png", &map);

    whitgl_ivec p_pos = {2, 2};
    player_create(p_pos, 0);
    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            if (rand() % 150 == 0 && MAP_TILE(&map, x, y) == 0) {
                whitgl_ivec rat_pos = {x, y};
                rat_create(rat_pos, &map);
            }
        }
    }

    cycles_to_astar = 0;
    actual_song_millis = 0;
    prev_actual_song_millis = 0;

    song_len = whitgl_loop_get_length(AMBIENT_MUSIC) / 1000.0f;
}

void game_cleanup() {
    rats_destroy_all(player);
    player_destroy();
    game_free_level(&map);
}

void game_start() {
    whitgl_loop_seek(AMBIENT_MUSIC, 0.0f);
    whitgl_loop_set_paused(AMBIENT_MUSIC, false);
    instruct("Get ready to move to the chune so I can calibrate my Zune!");
}

void game_stop() {
    whitgl_loop_set_paused(AMBIENT_MUSIC, true);
}

int game_update(float dt) {
    song_time = _fmod(song_time + dt, song_len);
    actual_song_millis = whitgl_loop_tell(AMBIENT_MUSIC);
    if (actual_song_millis != prev_actual_song_millis) {
        float actual_song_time = actual_song_millis / 1000.0f;
        float diff_ff, diff_rr;
        if (actual_song_time > song_time) {
            diff_ff = actual_song_time - song_time;
            diff_rr = song_time + song_len - actual_song_time;
        } else {
            diff_ff = actual_song_time + song_len - actual_song_time;
            diff_rr = song_time - actual_song_time;
        }
        if (diff_ff < diff_rr) {
            song_time += diff_ff / 2.0f;
        } else {
            song_time -= diff_rr / 2.0f;
        }
        song_time = _fmod(song_time, song_len);
        prev_actual_song_millis = actual_song_millis;
    }


    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    int next_cur_note = whitgl_imax(note, (int)((song_time - note_calib_offset) / secs_per_note));
    time_since_note = 0.0f;
    int next_state = player_update(player, dt);
    for (; note != next_cur_note; note++) {
        player_on_note(player, note);
        rats_on_note(player, note, true, &map);
        //notes_update(player, cur_loop, cur_note);
        if (note % (NOTES_PER_MEASURE / 16) == 0) {
            anim_objs_update();
        }
    }
    note_pop_text_update(dt);
    
    time_since_note = _fmod(song_time, secs_per_note);
    if(cycles_to_astar == 0) {
        rats_update(player, dt, note, true, &map);
        cycles_to_astar = 100;
    } else {
        rats_update(player, dt, note, false, &map);
        cycles_to_astar--;
    }
    phys_update(&map, dt);

    rats_prune(player);

    return next_state;
}

void game_frame() {
    frame(player, note);
}
