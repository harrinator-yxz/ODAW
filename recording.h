#ifndef RECORDING_H
#define RECORDING_H

#include <stdbool.h>
#include <glib.h>

char **get_input_device_names(void); // NULL-terminated array of device names
void set_recording_device(int device_index);

bool record_audio(const char *filename, int seconds);
bool play_audio(const char *filename);

#endif