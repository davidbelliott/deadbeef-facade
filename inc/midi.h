#ifndef MIDI_H
#define MIDI_H

typedef struct player_t player_t;
typedef struct map_t map_t;

enum {
    DIFFICULTY_EASY,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD,
    DIFFICULTY_ADVANCED
};

void midi_init();
void midi_cleanup();
void midi_start();
int midi_update(float dt);
void midi_input();
void midi_frame();

void midi_set_player(player_t *player, map_t *map);
void midi_set_difficulty(int difficulty, int length);

#endif
