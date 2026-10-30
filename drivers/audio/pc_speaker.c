#include "drivers/audio.h"
#include "drivers/hda.h"
#include "drivers/ac97.h"
#include "zarqaa/types.h"
#include "zarqaa/string.h"

static bool is_muted = false;
static int current_volume = 50;

void audio_init(void) {
    audio_stop();
    /* Apply the default software volume to the actual hardware */
    audio_set_volume(current_volume);
}

/* Pure 64-entry Sine Wave Table (50% amplitude to prevent clipping) */
static const int16_t sine_table_64[64] = {
    0, 1605, 3196, 4755, 6269, 7723, 9102, 10393,
    11584, 12664, 13622, 14448, 15136, 15678, 16068, 16304,
    16383, 16304, 16068, 15678, 15136, 14448, 13622, 12664,
    11584, 10393, 9102, 7723, 6269, 4755, 3196, 1605,
    0, -1605, -3196, -4755, -6269, -7723, -9102, -10393,
    -11584, -12664, -13622, -14448, -15136, -15678, -16068, -16304,
    -16383, -16304, -16068, -15678, -15136, -14448, -13622, -12664,
    -11584, -10393, -9102, -7723, -6269, -4755, -3196, -1605
};

void audio_play_freq(uint32_t freq) {
    if (is_muted || freq == 0) return;
    
    if (hda_is_available()) {
        uint16_t *buf = (uint16_t *)hda_get_ring_buffer_virt();
        if (!buf) return;
        uint32_t cycles = (freq * 32768) / 48000;
        if (cycles == 0) cycles = 1;
        uint32_t phase = 0;
        uint32_t phase_inc = cycles * 128;
        for (uint32_t i = 0; i < 32768; i++) {
            uint32_t index = (phase >> 16) % 64;
            int16_t sample = (sine_table_64[index] * (int32_t)current_volume) / 100;
            buf[i * 2]     = sample;
            buf[i * 2 + 1] = sample;
            phase += phase_inc;
        }
        hda_start_ring();
    } else {
        /* AC97 fallback: fill ring buffer with a short sine tone */
        uint8_t *rbuf = ac97_get_ring_buffer_virt();
        if (rbuf) {
            int16_t *sbuf = (int16_t *)rbuf;
            uint32_t cycles = (freq * 4096) / 48000;
            if (cycles == 0) cycles = 1;
            uint32_t phase = 0;
            uint32_t phase_inc = cycles * 128;
            /* Fill first 4096 bytes (2048 stereo samples) */
            for (uint32_t i = 0; i < 2048; i++) {
                uint32_t index = (phase >> 16) % 64;
                int16_t sample = (sine_table_64[index] * (int32_t)current_volume) / 100;
                sbuf[i * 2]     = sample;
                sbuf[i * 2 + 1] = sample;
                phase += phase_inc;
            }
            ac97_set_lvi(0);
            ac97_start_ring();
        }
    }
}

void audio_stop(void) {
    uint8_t b = inb(0x61);
    outb(0x61, b & 0xFC);
    
    if (hda_is_available()) {
        hda_stop();
        uint8_t *buf = hda_get_ring_buffer_virt();
        if (buf) {
            memset(buf, 0, 131072); // Clear the 128KB buffer
        }
    }
}

static int saved_volume = 50;

void audio_set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    
    current_volume = vol;
    
    if (current_volume == 0) {
        is_muted = true;
    } else {
        is_muted = false;
        saved_volume = current_volume;
    }
    
    if (is_muted) {
        audio_stop(); // Stop PC speaker
        hda_set_volume(0);
        ac97_set_volume(0);
    } else {
        hda_set_volume(current_volume);
        ac97_set_volume(current_volume);
    }
}

void audio_toggle_mute(void) {
    if (is_muted) {
        // Unmute: restore saved volume
        audio_set_volume(saved_volume > 0 ? saved_volume : 50);
    } else {
        // Mute
        audio_set_volume(0);
    }
}

int audio_get_volume(void) {
    return current_volume;
}

bool audio_is_muted(void) {
    return is_muted;
}

/* Simple UI Sounds */
// Note: In a real system, these would use PIT interrupts for async playback.
// Since PC speaker is synchronous blocking if we delay, we do very short beeps.

static void delay_short(volatile int count) {
    while(count--) {
        __asm__ __volatile__ ("pause");
    }
}

void audio_play_click(void) {
    if (is_muted) return;
    /* 
     * Disabled: Playing a continuous 800Hz sine wave on every UI click 
     * causes an annoying buzzing/humming sound because there is no 
     * async timer to stop it yet.
     */
    // audio_play_freq(800);
}

void audio_play_error(void) {
    if (hda_is_available()) return;
    audio_play_freq(300);
    delay_short(5000000);
    audio_stop();
}

void audio_play_startup(void) {
    /* Silence on startup - do not beep during boot with real HDA hardware */
    if (hda_is_available()) return;
    /* Only play startup chime if no HDA (e.g. QEMU PC Speaker only) */
    audio_play_freq(440);
    delay_short(8000000);
    audio_play_freq(554);
    delay_short(8000000);
    audio_play_freq(659);
    delay_short(10000000);
    audio_stop();
}
