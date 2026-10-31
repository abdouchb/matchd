/* Zarqaa OS 0.1 - Z-Player (Unique Audio Player)
Features: Animated Spectrum Visualizer, Custom Shapes, Dark UI */

#include "desktop/window_manager.h"
#include "kernel/heap.h"
#include "gui/graphics.h"
#include "drivers/framebuffer.h"
#include "drivers/pit.h"
#include "filesystem/vfs.h"
#include "drivers/ac97.h"
#include "drivers/hda.h"
#include "zarqaa/string.h"

extern void audio_play_freq(uint32_t freq);
extern void audio_stop(void);

struct wav_header {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt_chunk_marker[4];
    uint32_t length_of_fmt;
    uint16_t format_type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4];
    uint32_t data_size;
} __attribute__((packed));

struct zv_header {
    char magic[4];       // "ZVID"
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint8_t channels;
    uint16_t sample_rate;
    uint32_t total_frames;
} __attribute__((packed));

#define PLAYER_WIDTH 360
#define PLAYER_HEIGHT 480
#define NUM_BARS 32
#define FONT_WIDTH 8

// A simple pseudo-random number generator for the visualizer
// To avoid depending on <stdlib.h> in kernel space.

struct player_state {
    bool is_playing;
    bool was_playing;
    int current_track;
    int progress_sec;
    int duration_sec;
    int volume;
    int visualizer_bars[NUM_BARS];
    int target_bars[NUM_BARS];
    uint32_t last_tick;
    uint32_t last_sec_tick;
    char tracks[3][64];
    bool dragging_vol;
    
    // AC97/HDA WAV Playback state
    uint8_t *file_data; // Original allocated buffer
    uint8_t *wav_data;
    uint32_t wav_length;
    uint32_t wav_offset;
    uint8_t last_civ;
    
    // Z-Video Playback state
    bool is_video;
    uint16_t vid_width;
    uint16_t vid_height;
    uint8_t vid_fps;
    uint32_t vid_frames;
    uint8_t *vid_data;
};

// Draws a filled circle (useful for progress bar thumb)
static void draw_filled_circle(int xc, int yc, int r, color_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                fb_fill_rect(xc + x, yc + y, 1, 1, color);
            }
        }
    }
}

// Draws a play button (Right-pointing triangle)
static void draw_play_icon(int x, int y, color_t color) {
    for (int row = 0; row < 16; row++) {
        // Create a triangle shape: wide in middle, narrow at ends
        int width = (row <= 8) ? (row * 1.5) : ((16 - row) * 1.5);
        fb_fill_rect(x, y + row, width, 1, color);
    }
}

// Draws a pause button (Two vertical bars)
static void draw_pause_icon(int x, int y, color_t color) {
    fb_fill_rect(x, y, 5, 16, color);
    fb_fill_rect(x + 9, y, 5, 16, color);
}

// Draws Next/Prev Track icon
static void draw_skip_icon(int x, int y, bool forward, color_t color) {
    if (forward) {
        draw_play_icon(x, y, color);
        fb_fill_rect(x + 12, y, 3, 16, color);
    } else {
        // Reverse triangle
        for (int row = 0; row < 16; row++) {
            int width = (row <= 8) ? (row * 1.5) : ((16 - row) * 1.5);
            fb_fill_rect(x + 12 - width, y + row, width, 1, color);
        }
        fb_fill_rect(x - 3, y, 3, 16, color);
    }
}

static void update_visualizer(struct player_state *state) {
    if (!state->is_playing) {
        // Smoothly decay bars to zero if paused
        for (int i = 0; i < NUM_BARS; i++) {
            if (state->visualizer_bars[i] > 0) {
                state->visualizer_bars[i] -= 2;
                if (state->visualizer_bars[i] < 0) state->visualizer_bars[i] = 0;
            }
        }
        return;
    }

    // Generate new targets periodically (Real Audio Amplitude)
    if (pit_get_ticks() - state->last_tick > 2) { // Update ~50fps
        state->last_tick = pit_get_ticks();
        
        if (state->wav_data && state->wav_length > 0) {
            // Read from slightly behind wav_offset to match what is currently playing
            uint32_t play_offset = state->wav_offset;
            if (play_offset > 31 * 4096) play_offset -= 31 * 4096; // Offset DMA ahead time
            
            int16_t *pcm = (int16_t *)(state->wav_data + play_offset);
            uint32_t samples_left = (state->wav_length - play_offset) / 2;
            
            if (samples_left > 0) {
                int step = 20; // Spread across frequency/time
                for (int i = 0; i < NUM_BARS; i++) {
                    long sum = 0;
                    int count = 0;
                    int offset = i * step;
                    for (int j = 0; j < 10 && (uint32_t)(offset + j) < samples_left; j++) {
                        int16_t val = pcm[offset + j];
                        if (val < 0) val = -val;
                        sum += val;
                        count++;
                    }
                    if (count > 0) {
                        int avg = sum / count;
                        // Bass boost center bars
                        int center_bias = (NUM_BARS / 2) - i;
                        if (center_bias < 0) center_bias = -center_bias;
                        int boost = (16 - center_bias);
                        
                        int height = (avg * (60 + boost * 2)) / 8000;
                        if (height > 80) height = 80;
                        if (height < 2) height = 2;
                        state->target_bars[i] = height;
                    } else {
                        state->target_bars[i] = 2;
                    }
                }
            }
        }
    }
    
    // Advance time and update progress
    uint32_t ticks_now = pit_get_ticks();
    if (ticks_now - state->last_sec_tick >= 100) { // Approx 1 second
        state->progress_sec++;
        state->last_sec_tick = ticks_now;
        if (state->progress_sec >= state->duration_sec) {
            state->progress_sec = 0;
            state->wav_offset = 0; // Loop audio
            state->current_track = (state->current_track + 1) % 3;
        }
    }
    
    if (state->is_playing) {
        if (!state->was_playing) {
            bool use_hda = hda_is_available();
            if (use_hda) {
                hda_start_ring();
            } else {
                ac97_start_ring();
            }
            state->was_playing = true;
        }
        
        // Ring Buffer polling
        if (state->wav_data) {
            uint8_t civ;
            uint8_t *ring_virt;
            bool use_hda = hda_is_available();
            
            if (use_hda) {
                civ = (hda_get_lpib() / 4096) % 32;
                ring_virt = hda_get_ring_buffer_virt();
            } else {
                civ = ac97_get_civ();
                ring_virt = ac97_get_ring_buffer_virt();
            }
            
            if (!ring_virt) return; // Prevent kernel panic if driver failed to init
            
            // Fill until we are 31 chunks ahead of civ
            while (((state->last_civ + 1) % 32) != civ) {
                uint8_t *chunk_dest = ring_virt + (state->last_civ * 4096);
                
                if (state->wav_offset < state->wav_length) {
                    uint32_t copy_len = 4096;
                    if (state->wav_offset + copy_len > state->wav_length) {
                        copy_len = state->wav_length - state->wav_offset;
                    }
                    memcpy(chunk_dest, state->wav_data + state->wav_offset, copy_len);
                    if (copy_len < 4096) {
                        memset(chunk_dest + copy_len, 0, 4096 - copy_len);
                    }
                    state->wav_offset += copy_len;
                } else {
                    memset(chunk_dest, 0, 4096); // Silence
                }
                
                // Flush the cache so DMA can see the new audio data!
                for (uint32_t f = 0; f < 4096; f += 64) {
                    __asm__ volatile("clflush (%0)" :: "r"(chunk_dest + f) : "memory");
                }
                
                // Set Last Valid Index for AC97. HDA loops automatically.
                if (!use_hda) {
                    ac97_set_lvi(state->last_civ);
                }
                state->last_civ = (state->last_civ + 1) % 32;
            }
        }
    }

    // Interpolate towards targets for smooth animation
    for (int i = 0; i < NUM_BARS; i++) {
        if (state->visualizer_bars[i] < state->target_bars[i]) {
            state->visualizer_bars[i] += 4;
        } else if (state->visualizer_bars[i] > state->target_bars[i]) {
            state->visualizer_bars[i] -= 3;
        }
    }
}

// Helper to format MM:SS
static void format_time(int seconds, char* buffer) {
    int m = seconds / 60;
    int s = seconds % 60;
    buffer[0] = '0' + (m / 10);
    buffer[1] = '0' + (m % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (s / 10);
    buffer[4] = '0' + (s % 10);
    buffer[5] = '\0';
}

static void player_draw(struct window *win) {
    struct player_state *state = (struct player_state *)win->user_data;

    // Update logic
    update_visualizer(state);
    if (state->is_playing || state->visualizer_bars[0] > 0) {
        win->needs_redraw = true; // Trigger continuous animation
    }

    // 1. Dark Modern Background
    fb_fill_rect(0, 0, win->width, win->height, 0xFF0F172A); // Slate 900

    // 2. Album Art / Visualizer / Video Area
    int art_x = 40, art_y = 60;
    int art_w = 280, art_h = 220;

    // Glowing border & Glass background
    gfx_draw_rounded_rect_filled(art_x, art_y, art_w, art_h, 15, 0xFF1E293B);
    gfx_draw_rounded_rect(art_x, art_y, art_w, art_h, 15, 0xFF38BDF8); // Blue glow

    if (state->is_video && state->vid_data) {
        // Draw Z-Video Frame
        uint32_t frame_idx = 0;
        if (state->wav_length > 0 && state->vid_fps > 0) {
            uint32_t audio_bytes_per_sec = 48000 * 4; // 16-bit stereo
            uint32_t bytes_per_frame = audio_bytes_per_sec / state->vid_fps;
            if (bytes_per_frame > 0) {
                frame_idx = state->wav_offset / bytes_per_frame;
            }
            if (frame_idx >= state->vid_frames) frame_idx = state->vid_frames - 1;
        }
        
        uint16_t *frame_pixels = (uint16_t *)(state->vid_data + (frame_idx * state->vid_width * state->vid_height * 2));
        int start_x = art_x + (art_w - state->vid_width) / 2;
        int start_y = art_y + (art_h - state->vid_height) / 2;
        
        if (start_x < art_x) start_x = art_x;
        if (start_y < art_y) start_y = art_y;
        
        for (int y = 0; y < state->vid_height; y++) {
            if (start_y + y >= art_y + art_h) break;
            uint32_t *dest_row = win->content_buffer + ((start_y + y) * win->width) + start_x;
            uint16_t *src_row = frame_pixels + (y * state->vid_width);
            for (int x = 0; x < state->vid_width; x++) {
                if (start_x + x >= art_x + art_w) break;
                uint16_t rgb565 = src_row[x];
                uint32_t r = (rgb565 >> 11) & 0x1F;
                uint32_t g = (rgb565 >> 5) & 0x3F;
                uint32_t b = rgb565 & 0x1F;
                dest_row[x] = 0xFF000000 | ((r << 3) << 16) | ((g << 2) << 8) | (b << 3);
            }
        }
    } else {
        // Draw Visualizer Bars inside the box
        int bar_width = 6;
        int bar_gap = 2;
        int total_bars_w = NUM_BARS * (bar_width + bar_gap);
        int start_x = art_x + (art_w - total_bars_w) / 2;
        int start_y = art_y + art_h - 20;

        for (int i = 0; i < NUM_BARS; i++) {
            int h = state->visualizer_bars[i];
            if (h < 2) h = 2;
            
            int color = 0xFF38BDF8; // Blue
            if (h > 60) color = 0xFFF43F5E; // Red if high
            else if (h > 40) color = 0xFFFBBF24; // Yellow if medium
            
            fb_fill_rect(start_x + i * (bar_width + bar_gap), start_y - h, bar_width, h, color);
        }
    }

    // 3. Track Info
    const char* current_song = state->tracks[state->current_track];
    // Center text roughly (assuming 8px font width)
    int text_len = 0;
    while(current_song[text_len]) text_len++;
    int text_x = (win->width - (text_len * FONT_WIDTH)) / 2;

    fb_draw_string_transparent(text_x, 310, current_song, 0xFFF8FAFC);
    fb_draw_string_transparent((win->width - (14 * FONT_WIDTH)) / 2, 330, "Zarqaa Records", 0xFF94A3B8);

    // 4. Progress Bar
    int prog_x = 40, prog_y = 380, prog_w = 280, prog_h = 4;
    fb_fill_rect(prog_x, prog_y, prog_w, prog_h, 0xFF475569); // Slate 600
    if (state->duration_sec > 0) {
        int fill = (state->progress_sec * prog_w) / state->duration_sec;
        fb_fill_rect(prog_x, prog_y, fill, prog_h, 0xFF38BDF8); // Blue
        gfx_draw_circle_filled(prog_x + fill, prog_y + 2, 6, 0xFFFFFFFF); // Thumb
    }

    // 5. Volume Slider
    int vol_x = 100, vol_y = 455, vol_w = 160, vol_h = 4;
    fb_fill_rect(vol_x, vol_y, vol_w, vol_h, 0xFF475569); // Slate 600
    int vol_fill = (state->volume * vol_w) / 100;
    fb_fill_rect(vol_x, vol_y, vol_fill, vol_h, 0xFF38BDF8); // Blue
    gfx_draw_circle_filled(vol_x + vol_fill, vol_y + 2, 6, 0xFFFFFFFF); // Thumb
    
    // Draw Volume Icon
    gfx_draw_circle_filled(80, 457, 4, 0xFF94A3B8); // Tiny speaker icon representation

    // Timestamps
    char time_str[8];
    format_time(state->progress_sec, time_str);
    fb_draw_string_transparent(prog_x, prog_y + 10, time_str, 0xFF94A3B8);

    format_time(state->duration_sec, time_str);
    fb_draw_string_transparent(prog_x + prog_w - 40, prog_y + 10, time_str, 0xFF94A3B8);

    // 5. Controls (Prev, Play/Pause, Next)
    int ctrl_y = 415;
    draw_skip_icon(100, ctrl_y, false, 0xFFF8FAFC);

    // Play/Pause circular button
    draw_filled_circle(180, ctrl_y + 8, 20, 0xFF38BDF8);
    if (state->is_playing) {
        draw_pause_icon(175, ctrl_y, 0xFF0F172A);
    } else {
        draw_play_icon(175, ctrl_y, 0xFF0F172A);
    }

    draw_skip_icon(245, ctrl_y, true, 0xFFF8FAFC);
}

static void player_on_mouse(struct window *win, int mx, int my, bool left, bool right) {
    (void)right;
    struct player_state *state = (struct player_state *)win->user_data;
    if (!left) {
        state->dragging_vol = false;
        return;
    }

    // Play/Pause button hit detection (Circle center: 180, 428, radius: 20)
    int dx = mx - 180;
    int dy = my - 428;
    if (dx * dx + dy * dy <= 400) {
        if (state->is_playing) {
            if (hda_is_available()) hda_stop(); else ac97_stop();
        } else {
            if (hda_is_available()) hda_start_ring(); else ac97_start_ring();
        }
        state->is_playing = !state->is_playing;
        win->needs_redraw = true;
        return;
    }

    // Prev Button
    if (my >= 415 && my <= 440 && mx >= 90 && mx <= 120) {
        state->current_track = (state->current_track - 1);
        if (state->current_track < 0) state->current_track = 2;
        state->progress_sec = 0;
        win->needs_redraw = true;
        return;
    }

    // Next Button
    if (my >= 415 && my <= 440 && mx >= 240 && mx <= 270) {
        state->current_track = (state->current_track + 1) % 3;
        state->progress_sec = 0;
        state->wav_offset = 0;
        if (hda_is_available()) {
            hda_stop();
            hda_start_ring();
        } else {
            ac97_stop();
            ac97_start_ring();
        }
        state->last_civ = 0;
        win->needs_redraw = true;
        return;
    }

    // Progress Bar Seeking
    if (my >= 370 && my <= 395 && mx >= 40 && mx <= 320) {
        if (state->duration_sec > 0) {
            int fill = mx - 40;
            state->progress_sec = (fill * state->duration_sec) / 280;
            if (state->progress_sec < 0) state->progress_sec = 0;
            if (state->progress_sec > state->duration_sec) state->progress_sec = state->duration_sec;
            
            // Advance wav_offset
            if (state->wav_length > 0) {
                state->wav_offset = (state->progress_sec * state->wav_length) / state->duration_sec;
                // Align to 4 bytes
                state->wav_offset &= ~3;
            }
            
            win->needs_redraw = true;
        }
        return;
    }

    // Volume Slider Seeking
    if (my >= 445 && my <= 465 && mx >= 90 && mx <= 270) {
        state->dragging_vol = true;
        int vol_x = 100, vol_w = 160;
        int new_vol = ((mx - vol_x) * 100) / vol_w;
        if (new_vol < 0) new_vol = 0;
        if (new_vol > 100) new_vol = 100;
        
        if (state->volume != new_vol) {
            state->volume = new_vol;
            extern void audio_set_volume(int vol);
            audio_set_volume(state->volume);
        }
        win->needs_redraw = true;
        return;
    }
}

static void player_on_key(struct window *win, char key, uint8_t scancode) {
    (void)scancode;
    struct player_state *state = (struct player_state *)win->user_data;

    // Spacebar to toggle play/pause
    if (key == ' ') {
        state->is_playing = !state->is_playing;
        win->needs_redraw = true;
    }
}

static void player_on_close(struct window *win) {
    if (win->user_data) {
        struct player_state *state = (struct player_state *)win->user_data;
        if (state->file_data) {
            kfree(state->file_data);
        }
        if (hda_is_available()) {
            hda_stop();
        } else {
            ac97_stop();
        }
        kfree(win->user_data);
    }
}

struct window *audio_player_open_file(const char *path) {
    struct window *win = wm_create_window("Z-Player", 150, 80, PLAYER_WIDTH, PLAYER_HEIGHT);
    if (!win) return NULL;
    
    struct player_state *state = kcalloc(1, sizeof(struct player_state));
    
    struct vfs_node *file_node = NULL;
    
    if (path) {
        file_node = vfs_open(path);
        
        // Auto-convert path to .wav if the user clicks an mp3 (assuming a converted .wav exists in the same place)
        char real_path[128];
        strncpy(real_path, path, 127);
        real_path[127] = '\0';
        int len = strlen(real_path);
        if (len > 4 && strcmp(&real_path[len-4], ".mp3") == 0) {
            // Change to .wav
            real_path[len-4] = '.';
            real_path[len-3] = 'w';
            real_path[len-2] = 'a';
            real_path[len-1] = 'v';
            struct vfs_node *alt_node = vfs_open(real_path);
            if (alt_node) file_node = alt_node;
        }
    }
    
    if (file_node && file_node->type == VFS_FILE) {
        size_t file_size = file_node->size;
        state->file_data = kmalloc(file_size);
        if (!state->file_data) {
            kprintf("Failed to allocate memory for media file\n");
            kfree(state);
            return win;
        }
        
        vfs_read(file_node, state->file_data, file_size, 0);
        
        if (file_size >= 16 && memcmp(state->file_data, "ZVID", 4) == 0) {
            // Z-Video Format
            struct zv_header *zv = (struct zv_header *)state->file_data;
            state->is_video = true;
            state->vid_width = zv->width;
            state->vid_height = zv->height;
            state->vid_fps = zv->fps;
            state->vid_frames = zv->total_frames;
            
            uint32_t video_size = state->vid_frames * state->vid_width * state->vid_height * 2;
            state->vid_data = state->file_data + 16;
            
            state->wav_data = state->vid_data + video_size;
            state->wav_length = file_size - 16 - video_size;
            state->wav_offset = 0;
            
            state->duration_sec = state->vid_frames / state->vid_fps;
            kprintf("ZV loaded: %dx%d @ %dfps, %d sec\n", zv->width, zv->height, zv->fps, state->duration_sec);
            state->is_playing = true;
        } else if (file_size > sizeof(struct wav_header)) {
            // WAV Format parsing
            uint8_t *file_data = state->file_data;
            struct wav_header *wav = (struct wav_header *)file_data;
            if (strncmp(wav->riff, "RIFF", 4) == 0 && strncmp(wav->wave, "WAVE", 4) == 0) {
                // Scan for 'data' chunk
                uint8_t *ptr = file_data + 12; // Skip RIFF header
                uint32_t data_offset = 0;
                uint32_t data_length = 0;
                
                while (ptr < file_data + file_size - 8) {
                    if (strncmp((char *)ptr, "data", 4) == 0) {
                        data_length = *(uint32_t *)(ptr + 4);
                        data_offset = (ptr + 8) - file_data;
                        break;
                    }
                    // Skip this chunk
                    uint32_t chunk_size = *(uint32_t *)(ptr + 4);
                    ptr += 8 + chunk_size;
                }
                
                if (data_offset > 0) {
                    state->wav_data = file_data + data_offset;
                    state->wav_length = data_length;
                    if (state->wav_length > file_size - data_offset) {
                        state->wav_length = file_size - data_offset; // Prevent out-of-bounds
                    }
                    
                    state->duration_sec = (wav->byterate > 0) ? (state->wav_length / wav->byterate) : 0;
                    kprintf("WAV loaded: %d Hz, %d channels, %d sec\n", wav->sample_rate, wav->channels, state->duration_sec);
                    
                    // Autoplay when loaded
                    state->is_playing = true;
                } else {
                    kprintf("WAV loaded but no 'data' chunk found!\n");
                }
                // Adjust filename in tracks
                int path_len = 0;
                while(path[path_len]) path_len++;
                int last_slash = path_len;
                while(last_slash > 0 && path[last_slash-1] != '/') last_slash--;
                strncpy(state->tracks[0], &path[last_slash], 63);
            } else {
                kprintf("Not a valid WAV file: %s\n", path);
                kfree(file_data);
                state->file_data = NULL;
            }
        } else {
            kprintf("Failed to allocate memory for WAV file\n");
        }
    } else {
        kprintf("Failed to read WAV file: %s\n", path);
        state->duration_sec = 214; // Fallback
    }

    if (state->duration_sec == 0) {
        state->duration_sec = 214;
    }

    state->is_playing = true; // Auto-play!
    state->current_track = 0;
    state->progress_sec = 0;
    state->volume = 80;
    state->last_tick = pit_get_ticks();
    state->last_sec_tick = pit_get_ticks();

    if (path) {
        const char *filename = strrchr(path, '/');
        if (filename) filename++;
        else filename = path;
        strncpy(state->tracks[0], filename, 63);
        state->tracks[0][63] = '\0';
    } else {
        strcpy(state->tracks[0], "1. Zarqaa Kernel Boot Tune");
    }
    strcpy(state->tracks[1], "2. Memory Leak Blues");
    strcpy(state->tracks[2], "3. Page Fault in Nonpaged Area");

    win->user_data = state;
    win->on_draw = player_draw;
    win->on_mouse = player_on_mouse;
    win->on_key = player_on_key;
    win->on_close = player_on_close;

    return win;
}

struct window *audio_player_open(void) {
    return audio_player_open_file(NULL);
}
