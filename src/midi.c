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

#include "midi-parser.h"
#include "midi.h"
#include "game.h"
#include "graphics.h"
#include "music.h"
#include "rat.h"

#define MIDI_LEN 127     // number of notes in this clip

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
note_t notes[MIDI_LEN] = {{false}};
static player_t *player;
static map_t *map;

int damage_to_deal;
static int prev_music_beat; // What the cur_note was last time we updated
static int note;            // # of notes elapsed since starting the state

static int last_move_note;  // note when the player last made a move

static void translate_to_notes(struct midi_parser *parser);

static int vtime_to_beat(int16_t time_div, int64_t vtime) {
    if (time_div & 0x8000) {
        // Format: frames/sec | ticks/frame
        int8_t frames_per_sec = (time_div & 0x7F) >> 8;
        int8_t ticks_per_frame = (time_div & 0xFF);
        double time = (double)vtime / (double)(ticks_per_frame * frames_per_sec);
        int beat = (int)(time * 60.0f * BPM * 8.0f);
        return beat;
    } else {
        // Format: ticks per quarter note
        int16_t ticks_per_quarter_note = time_div & ~0x8000;
        int beat = (int)((double)vtime / (double)ticks_per_quarter_note * 8.0f);
        return beat;
        //time = quarter_notes / (double)BPM * 60.0;
    }
}

static int parse_file(const char *path)
{
    struct stat st;
    if (stat(path, &st)) {
        printf("stat(%s): %m\n", path);
        return 1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open(%s): %m\n", path);
        return 1;
    }

    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        printf("mmap(%s): %m\n", path);
        close(fd);
        return 1;
    }

    struct midi_parser parser;
    parser.state = MIDI_PARSER_INIT;
    parser.size  = st.st_size;
    parser.in    = mem;

    translate_to_notes(&parser);

    munmap(mem, st.st_size);
    close(fd);
    return 0;
}

void midi_init() {
    printf("Initializing MIDI gamestate\n");
    //parse_file("/home/david/gdrive/projects/deadbeef-facade/midi-parser/sample.mid");
    parse_file("sample2.mid");
    time = 0.0;
}

void midi_cleanup() {
}

void midi_start() {
    printf("Starting MIDI gamestate\n");
    time = 0.0;
    note = 0;
    prev_music_beat = music_get_cur_note();
    last_move_note = 0;

    for (int i = 0; i < MIDI_LEN; i++) {
        notes[i].exists = false;
    }
    for (int i = 1; i < MIDI_LEN / 4; i++) {
        notes[i * 4].exists = (rand() % 2 == 0);
        notes[i * 4].chan = rand() % 4;
        notes[i * 4].beat = i * 8;
    }

    clear_notif();
}

static void translate_to_notes(struct midi_parser *parser) {
    enum midi_status status;

    //
    //printf("%f\t/\t%f\n", parser_time, time);
    double parser_time;
    int beat;
    for (int i = 0; i < 256; ) {
        status = midi_parse(parser);
        switch (status) {
        case MIDI_PARSER_EOB:
        puts("eob");
        return;

        case MIDI_PARSER_ERROR:
        puts("error");
        return;

        case MIDI_PARSER_INIT:
        puts("init");
        break;

        case MIDI_PARSER_HEADER:
        printf("header\n");
        printf("  size: %d\n", parser->header.size);
        printf("  format: %d [%s]\n", parser->header.format, midi_file_format_name(parser->header.format));
        printf("  tracks count: %d\n", parser->header.tracks_count);
        printf("  time division: %d\n", parser->header.time_division);
        break;

        case MIDI_PARSER_TRACK:
        puts("track");
        printf("  length: %d\n", parser->track.size);
        break;

        case MIDI_PARSER_TRACK_MIDI:
        beat = vtime_to_beat(parser->header.time_division, parser->vtime);
        puts("track-midi");
        printf("  beat: %d\n", beat);
        printf("  vtime: %ld\n", parser->vtime);
        printf("  status: %d [%s]\n", parser->midi.status, midi_status_name(parser->midi.status));
        printf("  channel: %d\n", parser->midi.channel);
        printf("  param1: %d\n", parser->midi.param1);
        printf("  param2: %d\n", parser->midi.param2);

        if (parser->midi.status == MIDI_STATUS_NOTE_ON) {
            notes[i].exists = true;
            notes[i].chan = parser->midi.param1 % 4;
            notes[i].beat = beat;
        }
        break;

        case MIDI_PARSER_TRACK_META:
        printf("track-meta\n");
        printf("  time: %ld\n", parser->vtime);
        printf("  type: %d [%s]\n", parser->meta.type, midi_meta_name(parser->meta.type));
        printf("  length: %d\n", parser->meta.length);
        break;

        case MIDI_PARSER_TRACK_SYSEX:
        puts("track-sysex");
        printf("  time: %ld\n", parser->vtime);
        break;

        default:
        printf("unhandled state: %d\n", status);
        return;
        }
    }
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
        if (note >= MIDI_LEN) {
            return GAME_STATE_GAME;
        }

        if (note % (NOTES_PER_MEASURE / 8) == 0) {
            anim_objs_update();
        }
        prev_music_beat = next_music_beat;
    }
    rat_update(player->targeted_rat);
    return GAME_STATE_MIDI;
}

static whitgl_ivec get_note_pos(int i) {

    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    float frac_since_note = music_get_time_since_note() / secs_per_note;
    int chan = notes[i].chan;
    whitgl_ivec center_target_pos = {SCREEN_W / 2, SCREEN_H / 2};
    int dx = 0;
    int dy = 0;
    if (chan == 0) {
        center_target_pos.y -= NOTE_W;
        dy = 1;
    } else if (chan == 1) {
        center_target_pos.x -= NOTE_W;
        dx = 1;
    } else if (chan == 2) {
        dy = -1;
    } else if (chan == 3) {
        center_target_pos.x += NOTE_W;
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
    int w = SCREEN_H / 2 * (MIDI_LEN - note) / MIDI_LEN;
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
    whitgl_sys_color bg = {0, 0, 255, 128};
    whitgl_sys_color black = {0, 0, 0, 255};

    whitgl_iaabb bg_box = {{crosshairs_pos.x - 32, crosshairs_pos.y - 32}, {crosshairs_pos.x + 32, crosshairs_pos.y + 32}};
    //whitgl_sys_draw_iaabb(bg_box, black);

    whitgl_iaabb box = {{crosshairs_pos.x - NOTE_W / 2, crosshairs_pos.y - NOTE_W / 2}, {crosshairs_pos.x + NOTE_W / 2, crosshairs_pos.y + NOTE_W / 2}};
    box.a.y -= NOTE_W;
    box.b.y -= NOTE_W;
    whitgl_sys_draw_iaabb(box, bg);
    box.a.y += NOTE_W;
    box.b.y += NOTE_W;
    box.a.x -= NOTE_W;
    box.b.x -= NOTE_W;
    for (int i = 0; i < 3; i++) {
        whitgl_sys_draw_iaabb(box, bg);
        box.a.x += NOTE_W;
        box.b.x += NOTE_W;
    }

    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    float frac_since_note = music_get_time_since_note() / secs_per_note;

    for (int i = MAX(note - BAD_N_NOTES, 0); i < MIN(MIDI_LEN, note + LOOKAHEAD_TIME); i++) {
        if (notes[i].exists) {
            whitgl_ivec center_pos = get_note_pos(i);
            int x = center_pos.x - NOTE_W / 2;
            int y = center_pos.y - NOTE_W / 2;
            if (i <= note) {
                whitgl_iaabb iaabb = {{x, y}, {x + NOTE_W, y + NOTE_W}};
                whitgl_sys_draw_iaabb(iaabb, border);
            } else {
                whitgl_iaabb iaabb = {{x, y}, {x + NOTE_W, y + NOTE_W}};
                whitgl_sys_draw_iaabb(iaabb, black);
                whitgl_sys_draw_hollow_iaabb(iaabb, 1, border);
            }
        }
    }
}

void midi_frame() {

    whitgl_sys_color c = {0, 0, 0, 0};
    whitgl_set_shader_color(WHITGL_SHADER_EXTRA_0, 4, c);


    whitgl_sys_draw_init(0);
    whitgl_sys_enable_depth(true);
    whitgl_fvec pov_pos = {(float)player->look_pos.x / 256.0f + 0.5f, (float)player->look_pos.y / 256.0f + 0.5f};

    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    float frac_since_note = music_get_time_since_note() / secs_per_note;
    float angle = (player->look_angle) * whitgl_pi / 128.0f;

    draw_environment(pov_pos, angle, map);

    whitgl_sys_enable_depth(false);
    draw_note_bg();
    whitgl_sys_enable_depth(true);
    draw_entities(pov_pos, angle);
    whitgl_sys_enable_depth(false);
    draw_note_overlay();
    draw_notif(note);
    draw_explosions(note);
    draw_top_bar(note);
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
        for (int i = MAX(0, note - BAD_N_NOTES); i <= MIN(MIDI_LEN, note + BAD_N_NOTES); i++) {
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
