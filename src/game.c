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
int cur_loop = 0;
int cur_note = 0;

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

static int get_targeted_rat(int cur_targeted, whitgl_fvec player_pos, whitgl_fvec player_look) {
    int next_targeted = get_closest_targeted_rat(player_pos, player_look, &map);
    return next_targeted;
}

static void player_destroy() {
    phys_destroy(player->phys);
    free(player);
    player = NULL;
}

static void player_create(whitgl_fvec *pos, unsigned int angle) {
    if(player)
        player_destroy();
    player = (player_t*)malloc(sizeof(player_t));
    whitgl_fvec zero = {0.0, 0.0};
    player->phys = phys_create(pos, &zero, 0.2);
    player->angle = angle;
    player->look_angle = angle;
    player->look_direction = zero;
    player->move_direction = zero;
    player->move_dir = 0.0f;
    player->last_rotate_dir = 1;
    player->health = 100;
    player->damage_severity = 0.0f;
    player->targeted_rat = -1;
}

void game_load_level(char *levelname, map_t *map) {
    unsigned char *data;
    whitgl_int w = 0, h = 0;
    bool ret = whitgl_sys_load_png(levelname, &w, &h, &data);
    if (!ret) {
        map->data = NULL;
        return;
    }

    map->w = (int)w;
    map->h = (int)h;
    map->data = (unsigned char*)malloc(sizeof(unsigned char) * w * h);
    memset(map->data, 0, sizeof(unsigned char) * w * h);

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
        map->data[i] = tile;
    }
}

void game_free_level(map_t *map) {
    free(map->data);
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

static void draw_overlay(int cur_loop, int cur_note) {
    whitgl_sys_color top_col = {0, 0, 128, 255};

    whitgl_iaabb top_iaabb = {{0, 0}, {SCREEN_W, FONT_CHAR_H}};
    char path[256] = "[PATH: data/lvl/lvl1] [KEYS: RGB] [FPS: 60]";
    draw_window(path, top_iaabb, top_col);



    whitgl_sys_color col = {0, 0, 255, 255};
    whitgl_iaabb bottom_iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W, SCREEN_H}};
    whitgl_iaabb health_iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W * player->health / 100, SCREEN_H}};
    whitgl_sys_color black = {0, 0, 0, 255};
    char health[256];
    snprintf(health, 256, "HEALTH: %d", player->health);
    whitgl_sys_draw_iaabb(bottom_iaabb, black);
    draw_window(health, health_iaabb, col);

    /*whitgl_sys_color fill = {0, 0, 0, 255};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_iaabb zune_iaabb = {{8, 8}, {126, SCREEN_H - 8}};
    draw_window("My Zune", zune_iaabb, fill, border);

    int first_loop_shown = MIN(MAX(0, cur_loop - 2), NUM_LOOPS - 1 - 5);
    for (int i = 0; i < 4; i++) {
        int loop_idx = i + first_loop_shown;
        whitgl_sys_color fill = {0, 0, 0, 0};
        whitgl_sys_color outline = {0, 0, 0, 0};
        if (loop_idx == cur_loop || !loop_active) {
            int most_recent_note = 0;
            int total_notes = NOTES_PER_MEASURE * MEASURES_PER_LOOP;
            for ( ; most_recent_note < 16; most_recent_note ++) {
                int note_idx = _mod(cur_note - most_recent_note, total_notes);
                if (aloops[loop_idx].notes[note_idx].exists) {
                    break;
                }
            }
            whitgl_sys_color fill_active = {0, 0, (most_recent_note < 8 ? 255 * (8 - most_recent_note) / 8 : 0), 255};
            whitgl_sys_color outline_active = {0, 255, 255, 255};
            fill = fill_active;
            outline = outline_active;
        }
        whitgl_iaabb loop_iaabb = {{zune_iaabb.a.x, zune_iaabb.a.y + (FONT_CHAR_H + 9) * (i + 1)},
                                    {zune_iaabb.b.x, zune_iaabb.a.y + (FONT_CHAR_H + 9) * (i + 1) + FONT_CHAR_H + 9}};
        draw_window(aloops[loop_idx].name, loop_iaabb, fill, outline);
    }*/
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
        physics_obj *phys = target->phys;
        whitgl_fvec3 pos = {phys->pos.x, phys->pos.y, 0.5f};
        whitgl_fmat mv = whitgl_fmat_multiply(view, whitgl_fmat_translate(pos));
        whitgl_ivec window_coords = point_project(model_pos, mv, persp);
        if (whitgl_fvec_dot(player->look_direction, whitgl_fvec_sub(phys->pos, player->phys->pos)) < 0) {
            window_coords.x = window_coords.x >= SCREEN_W / 2 ? 8 : SCREEN_W - 8;
        }
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

static void draw_floors(whitgl_fvec *player_pos, whitgl_fmat view, whitgl_fmat persp) {
    for (int x = (int)MAX(0.0f, player_pos->x - MAX_DIST); x < (int)MIN(MAP_WIDTH, player_pos->x + MAX_DIST); x++) {
        for (int y = (int)MAX(0.0f, player_pos->y - MAX_DIST); y < (int)MIN(MAP_HEIGHT, player_pos->y + MAX_DIST); y++) {
            if (MAP_GET(&map, x, y) == 0) {
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
            if (!tile_walkable[MAP_GET(&map, x, y)]) {
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
            draw_wall(MAP_GET(&map, x, y), pos, view, persp);
            n_drawn++;
            drawn[y * map.h + x] = true;
        }
    }
    free(drawn);
    //WHITGL_LOG("n_drawn: %d", n_drawn);
}


static void frame(player_t *p, int cur_loop, int cur_note)
{
    printf("%f\t%f\n", p->look_angle, p->angle);
    if (p->angle != p->look_angle && p->last_rotate_dir == 1) {
        p->look_angle = (p->look_angle + 4) % 256;
    } else if (p->angle != p->look_angle) {
        p->look_angle = (p->look_angle - 4) % 256;
    }



    whitgl_sys_draw_init(0);

    whitgl_sys_enable_depth(true);

    whitgl_float fov = whitgl_pi/2;
    whitgl_fmat perspective = whitgl_fmat_perspective(fov, (float)SCREEN_W/(float)SCREEN_H, 0.1f, 100.0f);
    //whitgl_fmat perspective = whitgl_fmat_orthographic(-5.0f, 5.0f, 5.0f, -5.0f, -5.0f, 5.0f);
    whitgl_fvec3 up = {0,0,-1};
    whitgl_fvec3 camera_pos = {player->phys->pos.x,player->phys->pos.y,0.5f};
    whitgl_fvec3 camera_to = {camera_pos.x + player->look_direction.x, camera_pos.y + player->look_direction.y,0.5f};
    whitgl_fmat view = whitgl_fmat_lookAt(camera_pos, camera_to, up);
    //whitgl_fmat model_matrix = whitgl_fmat_translate(camera_to);
    //model_matrix = whitgl_fmat_multiply(model_matrix, whitgl_fmat_rot_z(time*3));

    //whitgl_sys_draw_model(0, WHITGL_SHADER_MODEL, model_matrix, view, perspective);
    whitgl_fvec3 skybox_pos = camera_pos;
    skybox_pos.z = -2.5f;
    //draw_skybox(skybox_pos, view, perspective);
    raycast(&player->phys->pos, p->look_angle * whitgl_pi / 128.0f, view, perspective);
    draw_floors(&player->phys->pos, view, perspective);
    draw_rats(view, perspective);

    whitgl_sys_enable_depth(false);

    draw_rat_overlays(player->targeted_rat, cur_note, time_since_note, view, perspective);

    //draw_notes(cur_loop, cur_note, time_since_note);
    draw_overlay(cur_loop, cur_note);

    // draw hurt overlay if needed
    int red_amt = player->damage_severity * 128;
    whitgl_sys_color c = {red_amt, 0, 0, 0};
    whitgl_set_shader_color(WHITGL_SHADER_POST, 0, c);

    whitgl_sys_draw_finish();
}


void player_deal_damage(player_t *p, int dmg) {
    p->damage_severity = 1.0f;
    p->health = MAX(0, p->health - dmg);
}

static int player_update(player_t *p, float dt)
{
    p->look_direction.x = cos(p->look_angle * whitgl_pi / 128.0f);
    p->look_direction.y = sin(p->look_angle * whitgl_pi / 128.0f);
    p->move_direction.x = cos(p->angle * whitgl_pi / 128.0f);
    p->move_direction.y = sin(p->angle * whitgl_pi / 128.0f);
    whitgl_fvec world_move_dir;
    world_move_dir.x = (p->move_direction.x*p->move_dir);
    world_move_dir.y = (p->move_direction.y*p->move_dir);
    set_vel(p->phys, world_move_dir, MOVE_SPEED);
    p->damage_severity = MAX(0.0f, p->damage_severity - dt);
    
    // Target if no rat targeted
    if (p->targeted_rat == -1) {
        p->targeted_rat = get_closest_visible_rat(p->phys->pos, &map);
    }

    // Test for portals, doors, etc
    if (MAP_GET(&map, (int)p->phys->pos.x, (int)p->phys->pos.y) == TILE_TYPE_PORTAL) {
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

    whitgl_float move = 0.0f;
    if(whitgl_input_held(WHITGL_INPUT_UP))
        move += 1.0;
    if(whitgl_input_held(WHITGL_INPUT_DOWN))
        move -= 1.0;
    /*if(whitgl_input_held(WHITGL_INPUT_RIGHT))
        move_dir.y -= 1.0;
    if(whitgl_input_held(WHITGL_INPUT_LEFT))
        move_dir.y += 1.0;*/
    if (whitgl_input_pressed(WHITGL_INPUT_RIGHT)) {
        p->angle = (p->angle + 64) % 256;
        p->last_rotate_dir = +1;
    }
    if (whitgl_input_pressed(WHITGL_INPUT_LEFT)) {
        p->angle = (p->angle - 64) % 256;
        p->last_rotate_dir = -1;
    }

    player->move_dir = move;

    if (whitgl_input_pressed(WHITGL_INPUT_A)) {
        int targeted = get_targeted_rat(player->targeted_rat, player->phys->pos, player->look_direction);
        if (targeted != -1) {
            player->targeted_rat = targeted;
        }
    }
    
    if (whitgl_input_pressed(WHITGL_INPUT_MOUSE_LEFT)) {
        if (p->targeted_rat != -1) {
            int best_candidate = NOTES_PER_MEASURE/8+1;
            int best_idx = -1;
            note_t *beat = rat_get(p->targeted_rat)->beat;
            for (int i = -NOTES_PER_MEASURE/8; i < NOTES_PER_MEASURE/8; i++) {
                int note_idx = _mod(cur_note + i, NOTES_PER_MEASURE * MEASURES_PER_LOOP);
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

    whitgl_fvec p_pos = {1.5, 2};
    player_create(&p_pos, 0);
    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            if (rand() % 150 == 0 && MAP_GET(&map, x, y) == 0) {
                whitgl_fvec rat_pos = {x + 0.5f, y + 0.5f};
                //rat_create(&rat_pos);
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
    //whitgl_loop_set_paused(AMBIENT_MUSIC, false);
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
    int next_cur_note = _mod((int)(song_time / secs_per_note), NOTES_PER_MEASURE * MEASURES_PER_LOOP);
    time_since_note = 0.0f;
    for (; cur_note != next_cur_note; cur_note = _mod(cur_note + 1, NOTES_PER_MEASURE * MEASURES_PER_LOOP)) {
        if (cur_note == 0) {
            whitgl_loop_seek(0, 0.0f);
        }
        //notes_update(player, cur_loop, cur_note);
        if (cur_note % (NOTES_PER_MEASURE / 16) == 0) {
            anim_objs_update();
        }
    }
    note_pop_text_update(dt);
    
    time_since_note = _fmod(song_time, secs_per_note);
    int next_state = player_update(player, dt);
    if(cycles_to_astar == 0) {
        rats_update(player, dt, cur_note, true, &map);
        cycles_to_astar = 100;
    } else {
        rats_update(player, dt, cur_note, false, &map);
        cycles_to_astar--;
    }
    phys_update(&map, dt);

    rats_prune(player);

    return next_state;
}

void game_frame() {
    frame(player, cur_loop, cur_note);
}
