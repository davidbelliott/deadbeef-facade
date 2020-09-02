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

#include "midi-parser.h"
#include "midi.h"
#include "music.h"
#include "rat.h"

#define MIDI_LEN 128     // number of notes in this clip

typedef struct note_t {
    bool exists;
    int beat;
    int chan;
} note_t;

double time;
note_t notes[MIDI_LEN] = {{false}};
static player_t *player;

int damage_to_deal;
static int prev_music_beat; // What the cur_note was last time we updated
static int note;            // # of notes elapsed since starting the state

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
    for (int i = 0; i < MIDI_LEN / 8; i++) {
        notes[i * 8].exists = true;
        notes[i * 8].chan = 0;//i % 4;
        notes[i * 8].beat = i * 8;
    }
    time = 0.0;
}

void midi_cleanup() {
}

void midi_start() {
    printf("Starting MIDI gamestate\n");
    time = 0.0;
    note = 0;
    prev_music_beat = music_get_cur_note();
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
        note++;
        if (note >= MIDI_LEN) {
            return GAME_STATE_GAME;
        }
        prev_music_beat = next_music_beat;
    }
    return GAME_STATE_MIDI;
}

static void draw_note_overlay() {
    whitgl_ivec crosshairs_pos = {SCREEN_W / 2, SCREEN_H / 2};
    whitgl_iaabb box = {{crosshairs_pos.x - 8, crosshairs_pos.y - 8}, {crosshairs_pos.x + 8, crosshairs_pos.y + 8}};

    whitgl_sys_color border = {255, 255, 255, 255};
    box.a.y -= 16;
    box.b.y -= 16;
    whitgl_sys_draw_hollow_iaabb(box, 1, border);
    box.a.y += 16;
    box.b.y += 16;
    box.a.x -= 16;
    box.b.x -= 16;
    for (int i = 0; i < 3; i++) {
        whitgl_sys_draw_hollow_iaabb(box, 1, border);
        box.a.x += 16;
        box.b.x += 16;
    }

    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    float frac_since_note = music_get_time_since_note() / secs_per_note;

    for (int i = note; i < MIDI_LEN && i < note + 16; i++) {
        if (notes[i].exists) {
            int chan = notes[i].chan;
            whitgl_ivec target_pos = {SCREEN_W / 2 - 8, SCREEN_H / 2 - 8};
            int dx = 0;
            int dy = 0;
            if (chan == 0) {
                target_pos.y -= 16;
                dy = 1;
            } else if (chan == 1) {
                target_pos.x -= 16;
                dx = 1;
            } else if (chan == 2) {
                dy = -1;
            } else if (chan == 3) {
                target_pos.x += 16;
                dx = -1;
            }
            int x = target_pos.x + dx * (note - i + frac_since_note) / 16.0 * SCREEN_W / 2.0;
            int y = target_pos.y + dy * (note - i + frac_since_note) / 16.0 * SCREEN_H / 2.0;
            if (i == note) {
                whitgl_iaabb iaabb = {target_pos, {target_pos.x + 16, target_pos.y + 16}};
                whitgl_sys_draw_iaabb(iaabb, border);
            } else {
                whitgl_iaabb iaabb = {{x, y}, {x + 16, y + 16}};
                whitgl_sys_draw_hollow_iaabb(iaabb, 1, border);
            }
        }
    }
}

void midi_frame() {
    whitgl_sys_draw_init(0);
    whitgl_sys_enable_depth(false);
    draw_note_overlay();
    whitgl_sys_draw_finish();
}
