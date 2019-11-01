#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

void game_init();
void game_cleanup();
void game_start();
int game_update(float dt);
void game_input();
void game_frame();
void game_pause(bool paused);

#endif
