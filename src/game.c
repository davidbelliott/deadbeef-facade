#include "game.h"
#include "common.h"
#include "map.h"
#include "anim.h"
#include "rat.h"
#include "key.h"
#include "intro.h"
#include "midi.h"
#include "music.h"
#include "ending.h"
#include "graphics.h"

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int lvl_grace_period = 32;

#define MOVE_NONE       0
#define MOVE_FORWARD    1
#define MOVE_BACKWARD   2
#define MOVE_TURN_LEFT  3
#define MOVE_TURN_RIGHT 4
#define MOVE_ATTACK     5

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

int earliest_active_note_offset = 0;
int note = 0;
int prev_note;
bool should_pause;

note_pop_text_t note_pop_text = { 0 };


player_t* player = NULL;

int level = 1;
map_t map = {0};

whitgl_float time_since_note = 0;

// Definitions of static helper functions

static void player_destroy() {
    free(player);
    player = NULL;
}

void game_from_midi() {
    if (player->targeted_rat != -1) {
        rat_kill(player->targeted_rat);
        player->targeted_rat = -1;
    }
    player->move = MOVE_NONE;
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
    player->keys = 0;
    player->move_time = -1;
}

void game_load_level(char *levelname, map_t *map) {
    int level_difficulty;
    char difficulty_path[256];
    snprintf(difficulty_path, 256, "data/lvl/lvl%d/difficulty", level);
    FILE *f = fopen(difficulty_path, "r");
    if (!fscanf(f, "%d", &level_difficulty))
        level_difficulty = 0;
    fclose(f);

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

    int data_idx, tile, entity;
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
        entity = ENTITY_TYPE_NONE;
        for (int j = 0; j < N_ENTITY_TYPES; j++) {
            if (entity_lvl_rgb[j][0] == (int)data[data_idx]
                    && entity_lvl_rgb[j][1] == (int)data[data_idx + 1]
                    && entity_lvl_rgb[j][2] == (int)data[data_idx + 2]) {
                entity = j;
                break;
            }
        }
        map->entities[i] = entity;

        whitgl_ivec pos = {i % map->w, i / map->w};
        switch (entity) {
            case ENTITY_TYPE_NONE:
                break;
            case ENTITY_TYPE_PLAYER:
                player_create(pos, 0);
                break;
            case ENTITY_TYPE_RAT:
            case ENTITY_TYPE_BLOCKER:
            case ENTITY_TYPE_WALKER:
            case ENTITY_TYPE_RUNNER:
            case ENTITY_TYPE_BOSS:
                rat_create(pos, map, entity, level_difficulty);
                break;
            case ENTITY_TYPE_RKEY:
                key_create(pos, KEY_R, map);
                break;
            case ENTITY_TYPE_GKEY:
                key_create(pos, KEY_G, map);
                break;
            case ENTITY_TYPE_BKEY:
                key_create(pos, KEY_B, map);
                break;
            default:
                printf("Not recognized\n");
                break;
        }
    }
}

void game_free_level(map_t *map) {
    free(map->tiles);
    free(map->entities);
}

void set_note_pop_text(char text[SHORT_TEXT_MAX_LEN]) {
    note_pop_text.exists = true;
    note_pop_text.life = 60.0f / music_get_cur_bpm();
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

static void draw_note_overlay() {
    if (note < lvl_grace_period - 8) {
        return; // grace period
    }

    int cur_music_note = music_get_cur_note();
    WHITGL_LOG("%d", cur_music_note);
    float song_time = music_get_song_time();
    float secs_per_note = (60.0f / music_get_cur_bpm() * 4 / NOTES_PER_MEASURE);
    float time_to_next_beat = secs_per_note * (8 - cur_music_note % 8)
        - music_get_time_since_note();
    float frac_to_next_beat = time_to_next_beat / secs_per_note / 8.0f;

    int width = SCREEN_W * frac_to_next_beat / 2.0f;
    //int height = (SCREEN_H - 2 * FONT_CHAR_H) * frac_to_next_beat;
    int height = SCREEN_W * frac_to_next_beat / 2.0f;

    whitgl_sys_color white = {255, 255, 255, 255};
    whitgl_iaabb iaabb = {{SCREEN_W / 2 - width / 2, SCREEN_H / 2 - height / 2},
        {SCREEN_W / 2 + width / 2, SCREEN_H / 2 + height / 2}};
    whitgl_sys_draw_hollow_iaabb(iaabb, 2, white);
}

static void draw_overlay(int cur_note) {

    draw_note_overlay();

    // Draw top bar
    draw_top_bar(cur_note, player);

    // Draw health bar
    draw_health_bar(cur_note, player);

    draw_notif(note, music_get_frac_since_note());

    draw_instr(note);
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

static void draw_sky() {
    int cur_note = music_get_cur_note();
    int most_recent_beat = note % 8;
    whitgl_iaabb full_iaabb = {{0, 0}, {SCREEN_W, SCREEN_H}};
    whitgl_sys_color fill_a = {0, 0, 30, 255};
    whitgl_sys_color fill_b = {30, 0, 0, 255};
    whitgl_sys_color sky_col = most_recent_beat < 2 ? fill_b : fill_a;

    whitgl_sys_draw_iaabb(full_iaabb, sky_col);
}

static void draw_crosshairs() {
    whitgl_ivec crosshairs_pos = {SCREEN_W / 2, SCREEN_H / 2};
    whitgl_iaabb box1 = {{crosshairs_pos.x - 1, crosshairs_pos.y - 4}, {crosshairs_pos.x + 1, crosshairs_pos.y + 4}};
    whitgl_iaabb box2 = {{crosshairs_pos.x - 4, crosshairs_pos.y - 1}, {crosshairs_pos.x + 4, crosshairs_pos.y + 1}};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_sys_draw_iaabb(box1, border);
    whitgl_sys_draw_iaabb(box2, border);
}

static void frame(player_t *p, int cur_note)
{
    whitgl_fvec pov_pos = {(float)player->look_pos.x / 256.0f + 0.5f, (float)player->look_pos.y / 256.0f + 0.5f};
    whitgl_sys_draw_init(0);

    whitgl_sys_enable_depth(false);

    draw_sky();

    whitgl_sys_enable_depth(true);


    //whitgl_fmat model_matrix = whitgl_fmat_translate(camera_to);
    //model_matrix = whitgl_fmat_multiply(model_matrix, whitgl_fmat_rot_z(time*3));

    //whitgl_sys_draw_model(0, WHITGL_SHADER_MODEL, model_matrix, view, perspective);
    float angle = p->look_angle * whitgl_pi / 128.0f;
    draw_environment(pov_pos, angle, &map);
    draw_entities(pov_pos, angle);

    whitgl_sys_enable_depth(false);


    draw_crosshairs();
    //draw_notes(cur_loop, cur_note, time_since_note);
    draw_overlay(cur_note);

    // draw move overlay if needed
    if (player->move_time > 0) {
        int elapsed = note - player->move_time;
        int color_amt = (elapsed < 4 ? (4 - elapsed) * 64 : 0);
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
    }

    whitgl_sys_draw_finish();
}


void player_deal_damage(player_t *p, int dmg, int note) {
    p->last_damage = note;
    p->health = MAX(0, p->health - dmg);
}

static void player_on_note(player_t *p, int note) {
    // TODO: fix this so it uses music cur_note instead!
    int cur_note = music_get_cur_note();
    if (cur_note % 8 == 4) {
        if (note >= lvl_grace_period) {
            if (!p->moved) {
                whitgl_sys_color col = {128, 0, 0, 255};
                notify(note, "YOU DIDN'T MOVE!", col);
                player_deal_damage(p, 2, note);
            }
            p->moved = false;
        }
    } else if (cur_note % 8 == 0 && note < lvl_grace_period) {
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
        notify(note, str, black);
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

    // Test for player having no health
    if (p->health == 0) {
        ending_set_text("Unfortunately, you died due to lack of vitality.");
        return GAME_STATE_ENDING;
    }
    
    // Test for portals, doors, etc
    if (MAP_TILE(&map, p->pos.x, p->pos.y) == TILE_TYPE_PORTAL) {
        WHITGL_LOG("next level");
        level++;
        intro_set_level(level);
        return GAME_STATE_INTRO;
    }

    if (p->targeted_rat != -1) {
        midi_set_player(p, &map);
        rat_t *r = rat_get(p->targeted_rat);
        int difficulty = r->difficulty;
        int len = (r->boss ? 512 : 256);
        midi_set_difficulty(difficulty, len);
        return GAME_STATE_MIDI;
    }

    return GAME_STATE_GAME;
}

void game_pause(bool paused) {
    music_set_paused(AMBIENT_MUSIC, paused);
}

void game_input()
{
    if (whitgl_input_pressed(WHITGL_INPUT_ESC)) {
        should_pause = true;
    } else {
        should_pause = false;
    }

    player_t *p = player;

    p->move = MOVE_NONE;
    if(whitgl_input_pressed(WHITGL_INPUT_UP)) {
        p->move = MOVE_FORWARD;
    } else if(whitgl_input_pressed(WHITGL_INPUT_DOWN)) {
        p->move = MOVE_BACKWARD;
    } else if (whitgl_input_pressed(WHITGL_INPUT_RIGHT)) {
        p->move = MOVE_TURN_RIGHT;
    } else if (whitgl_input_pressed(WHITGL_INPUT_LEFT)) {
        p->move = MOVE_TURN_LEFT;
    } else if (whitgl_input_pressed(WHITGL_INPUT_A)) {
        int targeted = get_closest_targeted_rat(
                player->pos, player->facing, &map);
        if (targeted != -1) {
            player->targeted_rat = targeted;
            p->move = MOVE_ATTACK;
        }
    }
    
    if (!p->moved && p->move != MOVE_NONE && note >= lvl_grace_period - 4) {

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

        // Compute how good of a move it is
        int cur_note_mod = music_get_cur_note() % 8;
        int best_n_notes = cur_note_mod >= 4 ? 7 - cur_note_mod : cur_note_mod;
        whitgl_sys_color green = {0, 128, 0, 255};
        whitgl_sys_color yellow = {128, 128, 0, 255};
        whitgl_sys_color red = {128, 0, 0, 255};
        whitgl_sys_color black = {0, 0, 0, 255};
        printf("Best notes: %d\n", best_n_notes);
        if (best_n_notes == 0) {
            notify(note, "GOOD", black);
            p->move_goodness = 2;
            p->health = MIN(100, p->health + 1);
        } else if (best_n_notes <= 1) {
            notify(note, "MEDIOCRE", black);
            p->move_goodness = 1;
        } else {
            notify(note, "BAD", black);
            player_deal_damage(p, 2, note);
            p->move_goodness = 0;
        }

        if (p->move == MOVE_FORWARD || p->move == MOVE_BACKWARD) {
            bool crash = true;
            if (tile_walkable[MAP_TILE(&map, newpos.x, newpos.y)]) {
                int entity = MAP_ENTITY(&map, newpos.x, newpos.y);
                if (entity == ENTITY_TYPE_NONE) {
                    crash = false;
                } else if (entity == ENTITY_TYPE_RAT) {
                    crash = true;
                } else if (entity == ENTITY_TYPE_RKEY) {
                    p->keys |= KEY_R;
                    key_destroy(key_at(newpos));
                    crash = false;
                } else if (entity == ENTITY_TYPE_GKEY) {
                    p->keys |= KEY_G;
                    key_destroy(key_at(newpos));
                    crash = false;
                } else if (entity == ENTITY_TYPE_BKEY) {
                    p->keys |= KEY_B;
                    key_destroy(key_at(newpos));
                    crash = false;
                }
            } else {
                if ((MAP_TILE(&map, newpos.x, newpos.y) == TILE_TYPE_RDOOR && p->keys & KEY_R) ||
                    (MAP_TILE(&map, newpos.x, newpos.y) == TILE_TYPE_GDOOR && p->keys & KEY_G) ||
                    (MAP_TILE(&map, newpos.x, newpos.y) == TILE_TYPE_BDOOR && p->keys & KEY_B)) {
                    // Unlock the door with a key
                    MAP_SET_TILE(&map, newpos.x, newpos.y, TILE_TYPE_FLOOR);
                    crash = false;
                    whitgl_sys_color green = {0, 128, 0, 255};
                    notify(note, "Door unlocked!", green);
                }
            }
            if (!crash) {
                MAP_SET_ENTITY(&map, p->pos.x, p->pos.y, ENTITY_TYPE_NONE);
                p->pos = newpos;
                MAP_SET_ENTITY(&map, newpos.x, newpos.y, ENTITY_TYPE_PLAYER);
            } else {
                whitgl_sys_color red = {128, 0, 0, 255};
                if (MAP_TILE(&map, newpos.x, newpos.y) == TILE_TYPE_RDOOR)
                    notify(note, "Door requires red key!", red);
                else if (MAP_TILE(&map, newpos.x, newpos.y) == TILE_TYPE_GDOOR)
                    notify(note, "Door requires green key!", red);
                else if (MAP_TILE(&map, newpos.x, newpos.y) == TILE_TYPE_BDOOR)
                    notify(note, "Door requires blue key!", red);
                else {
                    notify(note, "You crashed!", red);
                    player_deal_damage(p, 5, note);
                }
            }
        }
    }
}

void game_init() {
    whitgl_load_model(0, "data/obj/cube.wmd");
    whitgl_load_model(1, "data/obj/floor.wmd");
    whitgl_load_model(2, "data/obj/billboard.wmd");
    whitgl_load_model(3, "data/obj/skybox.wmd");
    whitgl_load_model(4, "data/obj/ceil.wmd");

}

void game_cleanup() {
    rats_destroy_all(player, &map);
    destroy_all_keys();
    player_destroy();
    game_free_level(&map);
}

void game_set_level(int level_in) {
    level = level_in;
    intro_set_level(level_in);
}

void game_start() {
    note = 0;
    prev_note = music_get_cur_note();
    instruct(note, "Get ready to move to the chune so I can calibrate my Zune!");

    char levelstr[256];
    snprintf(levelstr, 256, "data/lvl/lvl%d.png", level);
    WHITGL_LOG("Loading level: %s", levelstr);
    //intro_set_text(levelstr);
    game_load_level(levelstr, &map);
}

int game_update(float dt) {
    float secs_per_note = (60.0f / music_get_cur_bpm() * 4 / NOTES_PER_MEASURE);
    int next_note = music_get_cur_note();
    int song_len = music_get_song_len();
    if (prev_note != next_note) {
        if (prev_note < next_note)
            note += next_note - prev_note;
        else
            note += song_len - 1 - prev_note + next_note;
        //player_on_note(player, next_note);
        player_on_note(player, note);
        rats_on_note(player, note, true, &map);
        if (next_note % (NOTES_PER_MEASURE / 8) == 0) {
            anim_objs_update();
        }
        prev_note = next_note;
    }
    
    int next_state = player_update(player, note);
    rats_update(player, dt, note, true, &map);
    rats_prune(player, &map);

    if (should_pause) {
        return GAME_STATE_PAUSE;
    } else {
        return next_state;
    }
}

void game_frame() {
    frame(player, music_get_cur_note());
}
