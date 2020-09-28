#ifndef PAUSE_H
#define PAUSE_H

void pause_init();
void pause_cleanup();
void pause_start();
void pause_stop();
int pause_update(float dt);
void pause_input();
void pause_frame();

#endif
