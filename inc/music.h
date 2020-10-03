#include <whitgl/sound.h>

#define CUR_LVL_MUSIC 0

void music_init();
void music_update(float dt);
void music_set_paused(int snd, bool paused);
void music_play_from_beginning(int snd, double bpm);

int music_get_song_len();
float music_get_song_time();
int music_get_cur_note();
float music_get_time_since_note();
float music_get_frac_since_note();
double music_get_cur_bpm();
