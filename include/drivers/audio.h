#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

/* Basic PC Speaker Audio Subsystem */

/* Initialize the audio subsystem */
void audio_init(void);

/* Play a sound at a specific frequency (Hz). Use 0 to stop. */
void audio_play_freq(uint32_t freq);

/* Stop current sound */
void audio_stop(void);

/* Toggle Mute state */
void audio_toggle_mute(void);

/* Set Volume (0 = muted, 1+ = unmuted for PC Speaker) */
void audio_set_volume(int vol);
int audio_get_volume(void);

/* Check if muted */
bool audio_is_muted(void);

/* Play a preset UI effect */
void audio_play_click(void);
void audio_play_startup(void);
void audio_play_error(void);

#endif
