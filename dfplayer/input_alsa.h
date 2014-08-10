// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
//
// ALSA input module.

#ifndef __DFPLAYER_INPUT_ALSA_H
#define __DFPLAYER_INPUT_ALSA_H

#include <stdint.h>

typedef struct {
} AlsaInputHandle;

// Opens ALSA device for reading by name (e.g. "hw:0,0").
AlsaInputHandle *inp_alsa_init (const char* alsa_device, int rate);

void inp_alsa_cleanup (AlsaInputHandle* handle);

// Reads stereo PCM data, with sample rate of 44100.
// |data| must accomodate twice the the amount of |sampleCount|.
int inp_alsa_read (
	AlsaInputHandle* handle, float* data,
	int sampleCount, int *overrunCount);

#endif  // __DFPLAYER_INPUT_ALSA_H

