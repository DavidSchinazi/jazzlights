/* Libvisual-plugins - Standard plugins for libvisual
 *
 * Copyright (C) 2004, 2005, 2006 Vitaly V. Bursov <vitalyvb@urk,net>
 *
 * Authors: Vitaly V. Bursov <vitalyvb@urk,net>
 *	    Dennis Smit <ds@nerds-incorporated.org>
 *
 * $Id: input_alsa.c,v 1.23 2006/02/13 20:36:11 synap Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

// Original code taken from libvisual-plugins/plugins/input/alsa/input_alsa.c
// Igor Chernyshev: Changed to work independently from libvisual.

#include "input_alsa.h"

#include <alsa/version.h>
#if (SND_LIB_MAJOR == 0 && SND_LIB_MINOR == 9)
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#endif
#include <sys/types.h> // Workaround ALSA regression
#include <alsa/asoundlib.h>

#define PCM_BUF_SIZE 1024

typedef struct {
	snd_pcm_t *chandle;
	int loaded;
} alsaPrivate;

#define VISUAL_LOG_ERROR    1
#define VISUAL_LOG_WARNING  2
#define VISUAL_LOG_INFO     3

#define VISUAL_LITTLE_ENDIAN 1

#define TRUE    true
#define FALSE   false

#define visual_log(level, fmt, ...)                                      \
  fprintf(stderr, "input_alsa: " fmt, ##__VA_ARGS__);

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK(cond)                                                      \
  if (!(cond)) {                                                         \
    visual_log(VISUAL_LOG_ERROR, "EXITING with check-fail at %s (%s:%d)" \
            ". Condition = '" TOSTRING(cond) "'\n",                      \
            __FILE__, __FUNCTION__, __LINE__);                           \
    exit(-1);                                                            \
  }

static const int   inp_alsa_var_samplerate = 44100;
static const int   inp_alsa_var_channels   = 2;

static int inp_alsa_init_internal (const char *alsa_device, alsaPrivate *priv)
{
	snd_pcm_hw_params_t *hwparams = NULL;
	unsigned int rate = inp_alsa_var_samplerate;
	unsigned int exact_rate;
	unsigned int tmp;
	int dir = 0;
	int err;

	if ((err = snd_pcm_open(&priv->chandle, alsa_device,
			SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
		visual_log(VISUAL_LOG_ERROR,
			    "Record open error: %s", snd_strerror(err));
		return FALSE;
	}

	snd_pcm_hw_params_malloc(&hwparams);
	CHECK(hwparams != NULL);

	if (snd_pcm_hw_params_any(priv->chandle, hwparams) < 0) {
		visual_log(VISUAL_LOG_ERROR,
			   "Cannot configure this PCM device");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

	if (snd_pcm_hw_params_set_access(priv->chandle, hwparams,
					 SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		visual_log(VISUAL_LOG_ERROR, "Error setting access");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

#if VISUAL_LITTLE_ENDIAN == 1
	if (snd_pcm_hw_params_set_format(priv->chandle, hwparams,
					 SND_PCM_FORMAT_S16_LE) < 0) {
#else
	if (snd_pcm_hw_params_set_format(priv->chandle, hwparams,
					 SND_PCM_FORMAT_S16_BE) < 0) {
#endif
		visual_log(VISUAL_LOG_ERROR, "Error setting format");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

	exact_rate = rate;

	if (snd_pcm_hw_params_set_rate_near(priv->chandle, hwparams,
					    &exact_rate, &dir) < 0) {
		visual_log(VISUAL_LOG_ERROR, "Error setting rate");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}
	if (exact_rate != rate) {
                visual_log(VISUAL_LOG_INFO,
                           "The rate %d Hz is not supported by your " \
                           "hardware.\n" \
                           "==> Using %d Hz instead", rate, exact_rate);
	}

	if (snd_pcm_hw_params_set_channels(priv->chandle, hwparams,
					   inp_alsa_var_channels) < 0) {
		visual_log(VISUAL_LOG_ERROR, "Error setting channels");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

	/* Setup a large buffer */

	tmp = 1000000;
	if (snd_pcm_hw_params_set_period_time_near(priv->chandle, hwparams, &tmp, &dir) < 0) {
		visual_log(VISUAL_LOG_ERROR, "Error setting period time");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

	tmp = 1000000*4;
	if (snd_pcm_hw_params_set_buffer_time_near(priv->chandle, hwparams, &tmp, &dir) < 0){
		visual_log(VISUAL_LOG_ERROR, "Error setting buffer time");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}


	if (snd_pcm_hw_params(priv->chandle, hwparams) < 0) {
		visual_log(VISUAL_LOG_ERROR, "Error setting HW params");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

	if (snd_pcm_prepare(priv->chandle) < 0) {
		visual_log(VISUAL_LOG_ERROR, "Failed to prepare interface");
		snd_pcm_hw_params_free(hwparams);
		return FALSE;
	}

	snd_pcm_hw_params_free(hwparams);

	priv->loaded = TRUE;

	return TRUE;
}

AlsaInputHandle *inp_alsa_init (const char* alsa_device)
{
	alsaPrivate *priv = new alsaPrivate();
	if (!inp_alsa_init_internal (alsa_device, priv)) {
		delete priv;
		return NULL;
	}
	return (AlsaInputHandle*) priv;
}

void inp_alsa_cleanup (AlsaInputHandle* handle)
{
	alsaPrivate *priv =  (alsaPrivate*) handle;

	if (priv->loaded) {
		snd_pcm_close(priv->chandle);
	}

	delete priv;
}

int inp_alsa_read (
	AlsaInputHandle* handle, int16_t *data,
	int sampleCount, int *overrunCount)
{
	alsaPrivate *priv =  (alsaPrivate*) handle;

	*overrunCount = 0;
	int rcnt;
	while (true) {
		rcnt = snd_pcm_readi(priv->chandle, data, sampleCount);
		if (rcnt >= 0) {
			return rcnt;
		}

		if (rcnt == -EPIPE) {
			*overrunCount += 1;
			visual_log(VISUAL_LOG_WARNING, "ALSA: Buffer Overrun");
			if (snd_pcm_prepare(priv->chandle) < 0) {
				visual_log(VISUAL_LOG_ERROR,
						"Failed to prepare interface");
				return -1;
			}
			continue;
		}

		visual_log(VISUAL_LOG_ERROR,
			    "snd_pcm_readi: %s", snd_strerror(rcnt));
		return -1;
	}

	return 0;
}

