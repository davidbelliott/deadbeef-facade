#include "common.h"

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "midi.h"
#include "game.h"
#include "graphics.h"
#include "music.h"
#include "rat.h"
#include "ending.h"

#define MAX_MIDI_LEN 1024     // number of notes in this clip

#define NOTE_W 20

#define BAD_N_NOTES         4
#define MEDIOCRE_N_NOTES    2
#define GOOD_N_NOTES        1

#define LOOKAHEAD_TIME  64

typedef struct note_t {
    bool exists;
    int beat;
    int chan;
} note_t;

double time;
note_t notes[MAX_MIDI_LEN] = {{false}};
static player_t *player;
static map_t *map;

int damage_to_deal;
static int prev_music_beat; // What the cur_note was last time we updated
static int note;            // # of notes elapsed since starting the state

static int last_move_note;  // note when the player last made a move

int difficulty;
int length;

void midi_init() {
    printf("Initializing MIDI gamestate\n");
    //parse_file("/home/david/gdrive/projects/deadbeef-facade/midi-parser/sample.mid");
    time = 0.0;
    difficulty = DIFFICULTY_EASY;
}

void midi_cleanup() {
}

void midi_start() {
    printf("Starting MIDI gamestate\n");
    time = 0.0;
    note = 0;
    prev_music_beat = music_get_cur_note();
    last_move_note = 0;

    for (int i = 0; i < MAX_MIDI_LEN; i++) {
        notes[i].exists = false;
    }
    int main_interval;
    int half_note_chance;
    switch (difficulty) {
        case DIFFICULTY_EASY:
        default:
            main_interval = 4;
            half_note_chance = 8;
            break;
        case DIFFICULTY_MEDIUM:
            main_interval = 4;
            half_note_chance = 4;
            break;
        case DIFFICULTY_HARD:
            main_interval = 4;
            half_note_chance = 2;
            break;
        case DIFFICULTY_ADVANCED:
            main_interval = 2;
            half_note_chance = 2;
            break;
    }
    for (int i = 1; i < length / main_interval; i++) {
        notes[i * main_interval].exists = (rand() % half_note_chance == 0 || i % 2 == 0);
        notes[i * main_interval].chan = rand() % 4;
        notes[i * main_interval].beat = i * main_interval;
    }

    clear_notif();
}

int midi_update(float dt) {
    int next_music_beat = music_get_cur_note();
    if (next_music_beat != prev_music_beat) {
        int idx = note - BAD_N_NOTES;
        if (idx >= 0 && notes[idx].exists) {
            whitgl_sys_color red = {128, 0, 0, 255};
            notify(note, "MISS", red);
            player_deal_damage(player, 4, note);
        }
        note++;
        if (note >= length) {
            return GAME_STATE_GAME;
        }

        if (note % (NOTES_PER_MEASURE / 8) == 0) {
            anim_objs_update();
        }
        prev_music_beat = next_music_beat;
    }
    rat_update(player->targeted_rat);

    // Test for player having no health
    if (player->health == 0) {
        ending_set_text("Unfortunately, in the midst of a MIDI sequence you died due to lack of vitality.");
        return GAME_STATE_ENDING;
    }
    return GAME_STATE_MIDI;
}

static whitgl_ivec get_note_pos(int i) {

    float frac_since_note = music_get_frac_since_note();
    int chan = notes[i].chan;
    whitgl_ivec center_target_pos = {SCREEN_W / 2, SCREEN_H / 2};
    int dx = 0;
    int dy = 0;
    if (chan == 0) {
        center_target_pos.y -= NOTE_W - 1;
        dy = 1;
    } else if (chan == 1) {
        center_target_pos.x -= NOTE_W - 1;
        dx = 1;
    } else if (chan == 2) {
        dy = -1;
    } else if (chan == 3) {
        center_target_pos.x += NOTE_W - 1;
        dx = -1;
    }
    if (i <= note) {
        return center_target_pos;
    } else {
        int x = center_target_pos.x + dx * (note - i + frac_since_note) * SCREEN_W / LOOKAHEAD_TIME;
        int y = center_target_pos.y + dy * (note - i + frac_since_note) * SCREEN_W / LOOKAHEAD_TIME;
        whitgl_ivec pos = {x, y};
        return pos;
    }
}

static void draw_note_bg() {
    whitgl_ivec crosshairs_pos = {SCREEN_W / 2, SCREEN_H / 2};

    int r = 128 * (note % 2);
    int w = SCREEN_H / 2 * (MAX_MIDI_LEN - note) / MAX_MIDI_LEN;
    //whitgl_sys_color bg = {r, 0, 128 - r, 255};
    whitgl_sys_color bg = {32, 0, 128, 255};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_sys_color black = {0, 0, 0, 128};

    int elapsed = note - last_move_note;
    int color_amt = (elapsed < 3 ? (3 - elapsed) * 64 : 0);
    whitgl_sys_color c = {0, 0, 0, 128};
    switch (player->move_goodness) {
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

    whitgl_iaabb cross1 = {{0, 0}, {SCREEN_W, SCREEN_H}};
    //whitgl_iaabb cross1 = {{SCREEN_W / 2 - w, 0}, {SCREEN_W / 2 + w, SCREEN_H}};
    //whitgl_iaabb cross2 = {{0, SCREEN_H / 2 - w}, {SCREEN_W, SCREEN_H / 2 + w}};
    whitgl_sys_draw_iaabb(cross1, c);
    //whitgl_sys_draw_iaabb(cross2, black);
}

static void draw_note_overlay() {
    whitgl_ivec crosshairs_pos = {SCREEN_W / 2, SCREEN_H / 2};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_sys_color bg = {0, 0, 0, 255};
    whitgl_sys_color black = {0, 0, 0, 255};

    whitgl_iaabb bg_box = {{crosshairs_pos.x - 30, crosshairs_pos.y - 30}, {crosshairs_pos.x + 30, crosshairs_pos.y + 30}};
    //whitgl_sys_draw_iaabb(bg_box, black);

    whitgl_iaabb box = {{crosshairs_pos.x - NOTE_W / 2, crosshairs_pos.y - NOTE_W / 2}, {crosshairs_pos.x + NOTE_W / 2, crosshairs_pos.y + NOTE_W / 2}};
    box.a.y -= NOTE_W - 1;
    box.b.y -= NOTE_W - 1;
    whitgl_sys_draw_iaabb(box, bg);
    whitgl_sys_draw_hollow_iaabb(box, 1, border);
    box.a.y += NOTE_W - 1;
    box.b.y += NOTE_W - 1;
    box.a.x -= NOTE_W - 1;
    box.b.x -= NOTE_W - 1;
    for (int i = 0; i < 3; i++) {
        whitgl_sys_draw_iaabb(box, bg);
        whitgl_sys_draw_hollow_iaabb(box, 1, border);
        box.a.x += NOTE_W - 1;
        box.b.x += NOTE_W - 1;
    }

    float frac_since_note = music_get_frac_since_note();

    for (int i = MAX(note - BAD_N_NOTES, 0); i < MIN(MAX_MIDI_LEN, note + LOOKAHEAD_TIME); i++) {
        if (notes[i].exists) {
            whitgl_ivec center_pos = get_note_pos(i);
            int half_w = NOTE_W / 2;// * (1.0 + (i - note) / 16.0f);
            whitgl_sys_color note_color = {128, 0, 128, 255};
            if (i <= note) {
                int x = center_pos.x - NOTE_W / 2;
                int y = center_pos.y - NOTE_W / 2;
                whitgl_iaabb iaabb = {{x, y}, {x + NOTE_W, y + NOTE_W}};
                whitgl_sys_draw_iaabb(iaabb, border);
            } else {
                int x = center_pos.x - half_w;
                int y = center_pos.y - half_w;
                whitgl_iaabb iaabb = {{x, y}, {x + 2 * half_w, y + 2 * half_w}};
                whitgl_sys_draw_iaabb(iaabb, note_color);
                whitgl_sys_draw_hollow_iaabb(iaabb, 1, border);
            }
        }
    }
}

void draw_countdown() {
    int eighth_notes_remaining = (length - note) / 4;
    char str[128];
    float frac = (float)eighth_notes_remaining / (float)(length / 4);
    snprintf(str, 128, "[ENEMY: %.2f]", frac);
    int w = SCREEN_W * frac;
    int top_y = FONT_CHAR_H;
    whitgl_iaabb bg_iaabb = {{0, top_y}, {SCREEN_W, top_y + FONT_CHAR_H}};
    whitgl_iaabb health_iaabb = {{0, top_y}, {w, top_y + FONT_CHAR_H}};
    whitgl_sys_color black = {0, 0, 0, 255};
    whitgl_sys_color health = {128, 0, 0, 255};
    whitgl_sys_draw_iaabb(bg_iaabb, black);
    draw_window(str, health_iaabb, health);
}

void midi_frame() {

    whitgl_sys_color c = {0, 0, 0, 0};
    whitgl_set_shader_color(WHITGL_SHADER_EXTRA_0, 4, c);


    whitgl_sys_draw_init(0);
    whitgl_sys_enable_depth(true);
    whitgl_fvec pov_pos = {(float)player->look_pos.x / 256.0f + 0.5f, (float)player->look_pos.y / 256.0f + 0.5f};

    float frac_since_note = music_get_frac_since_note();
    float angle = (player->look_angle) * whitgl_pi / 128.0f;

    draw_environment(pov_pos, angle, map);

    whitgl_sys_enable_depth(false);
    draw_note_bg();
    whitgl_sys_enable_depth(true);
    draw_entities(pov_pos, angle);
    whitgl_sys_enable_depth(false);
    draw_note_overlay();
    draw_countdown();
    draw_notif(note, frac_since_note);
    draw_explosions(note, frac_since_note);
    draw_top_bar(note, player);
    draw_health_bar(note, player);
    whitgl_sys_draw_finish();
}

void midi_set_player(player_t *player_in, map_t *map_in) {
    player = player_in;
    map = map_in;
    rat_t *r = rat_get(player->targeted_rat);
    rat_try_move(r, whitgl_ivec_add(player->pos, player->facing), map);
}

void midi_input() {
    int chan_pressed = -1;
    if (whitgl_input_pressed(WHITGL_INPUT_UP)) {
        chan_pressed = 0;
    } else if (whitgl_input_pressed(WHITGL_INPUT_LEFT)) {
        chan_pressed = 1;
    } else if (whitgl_input_pressed(WHITGL_INPUT_DOWN)) {
        chan_pressed = 2;
    } else if (whitgl_input_pressed(WHITGL_INPUT_RIGHT)) {
        chan_pressed = 3;
    }
    if (chan_pressed != -1) {
        int closest_note_idx = -1;
        int closest_dist = BAD_N_NOTES + 1;
        for (int i = MAX(0, note - BAD_N_NOTES); i <= MIN(MAX_MIDI_LEN, note + BAD_N_NOTES); i++) {
            if (notes[i].exists && notes[i].chan == chan_pressed) {
                int dist = abs(i - note);
                if (dist < closest_dist) {
                    closest_note_idx = i;
                    closest_dist = dist;
                }
            }
        }
        if (closest_note_idx != -1) {
            add_explosion(note, get_note_pos(closest_note_idx));
            notes[closest_note_idx].exists = false;

            whitgl_sys_color black = {0, 0, 0, 255};
            if (closest_dist <= GOOD_N_NOTES) {
                notify(note, "GOOD", black);
                player->move_goodness = 2;
            } else if (closest_dist <= MEDIOCRE_N_NOTES) {
                notify(note, "MEDIOCRE", black);
                player->move_goodness = 1;
            } else {
                notify(note, "BAD", black);
                player_deal_damage(player, 2, note);
                player->move_goodness = 0;
            }
            last_move_note = note;
        }
    }
}

void midi_set_difficulty(int difficulty_in, int length_in) {
    difficulty = difficulty_in;
    length = length_in;
}
