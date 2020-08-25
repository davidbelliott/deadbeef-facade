#ifndef MIDI_H
#define MIDI_H

void midi_init();
void midi_cleanup();
void midi_start();
int midi_update(float dt);
void midi_frame();

#endif
