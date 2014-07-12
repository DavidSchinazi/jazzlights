// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
//
// ALSA input module.

#ifndef __DFPLAYER_INPUT_ALSA_H
#define __DFPLAYER_INPUT_ALSA_H

typedef struct {
} InputHandle;

// Opens ALSA device for reading by name (e.g. "hw:0,0").
InputHandle *inp_alsa_init (const char* alsa_device);

void inp_alsa_cleanup (InputHandle* handle);

// Reads stereo PCM data, with sample rate of 44100.
// |data| must accomodate twice the the amount of |sampleCount|.
int inp_alsa_read (
	InputHandle* handle, int16_t *data, int sampleCount, int *overrunCount);

#endif  // __DFPLAYER_INPUT_ALSA_H

