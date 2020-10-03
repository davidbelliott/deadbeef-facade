#ifndef ENDING_H
#define ENDING_H

#include <stdbool.h>

void ending_init();
void ending_cleanup();
void ending_start();
int ending_update(float dt);
void ending_input();
void ending_frame();
void ending_pause(bool paused);

void ending_set_text(char *newtext);

#endif
