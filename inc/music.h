#include <whitgl/sound.h>

#define AMBIENT_MUSIC -1

void music_init();
void music_update(float dt);
void music_set_paused(int snd, bool paused);
void music_play_from_beginning(int snd);

float music_get_song_time();
int music_get_cur_note();
float music_get_time_since_note();
