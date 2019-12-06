#ifndef INTRO_H
#define INTRO_H

#include <stdbool.h>

void intro_init();
void intro_cleanup();
void intro_start();
int intro_update(float dt);
void intro_input();
void intro_frame();
void intro_pause(bool paused);

void intro_set_text(char *newtext);

#endif
