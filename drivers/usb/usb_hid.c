#include "drivers/usb/usb_hid.h"
#include "kernel/early_boot_log.h"
#include "zarqaa/string.h"

/* --- Keyboard State --- */
#define KBD_BUF_SIZE 256
static struct key_event kbd_buffer[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;
static volatile bool key_state[128];
static volatile bool shift_pressed = false;
static volatile bool ctrl_pressed = false;
static volatile bool alt_pressed = false;
static volatile bool caps_lock = false;
static volatile bool num_lock = true; // Num Lock usually defaults to ON

static const char scancode_to_char[128] = {
    [0x00] = 0,
    [0x01] = 27,   // ESC
    [0x02] = '1',  [0x03] = '2',  [0x04] = '3',  [0x05] = '4',
    [0x06] = '5',  [0x07] = '6',  [0x08] = '7',  [0x09] = '8',
    [0x0A] = '9',  [0x0B] = '0',  [0x0C] = '-',  [0x0D] = '=',
    [0x0E] = '\b', // Backspace
    [0x0F] = '\t', // Tab
    [0x10] = 'q',  [0x11] = 'w',  [0x12] = 'e',  [0x13] = 'r',
    [0x14] = 't',  [0x15] = 'y',  [0x16] = 'u',  [0x17] = 'i',
    [0x18] = 'o',  [0x19] = 'p',  [0x1A] = '[',  [0x1B] = ']',
    [0x1C] = '\n', // Enter
    [0x1D] = 0,    // Left Ctrl
    [0x1E] = 'a',  [0x1F] = 's',  [0x20] = 'd',  [0x21] = 'f',
    [0x22] = 'g',  [0x23] = 'h',  [0x24] = 'j',  [0x25] = 'k',
    [0x26] = 'l',  [0x27] = ';',  [0x28] = '\'', [0x29] = '`',
    [0x2A] = 0,    // Left Shift
    [0x2B] = '\\', [0x2C] = 'z',  [0x2D] = 'x',  [0x2E] = 'c',
    [0x2F] = 'v',  [0x30] = 'b',  [0x31] = 'n',  [0x32] = 'm',
    [0x33] = ',',  [0x34] = '.',  [0x35] = '/',  [0x36] = 0, // Right Shift
    [0x37] = '*',  // Keypad *
    [0x38] = 0,    // Left Alt
    [0x39] = ' ',  // Space
    [0x3A] = 0,    // CapsLock
    [0x45] = 0,    // NumLock
    [0x47] = 0,    // Home / 7
    [0x48] = 0,    // Up / 8
    [0x49] = 0,    // PgUp / 9
    [0x4A] = '-',  // Keypad -
    [0x4B] = 0,    // Left / 4
    [0x4C] = 0,    // Keypad 5
    [0x4D] = 0,    // Right / 6
    [0x4E] = '+',  // Keypad +
    [0x4F] = 0,    // End / 1
    [0x50] = 0,    // Down / 2
    [0x51] = 0,    // PgDn / 3
    [0x52] = 0,    // Insert / 0
    [0x53] = 0     // Delete / .
};

static const char scancode_to_char_shift[128] = {
    [0x00] = 0,
    [0x01] = 27,   // ESC
    [0x02] = '!',  [0x03] = '@',  [0x04] = '#',  [0x05] = '$',
    [0x06] = '%',  [0x07] = '^',  [0x08] = '&',  [0x09] = '*',
    [0x0A] = '(',  [0x0B] = ')',  [0x0C] = '_',  [0x0D] = '+',
    [0x0E] = '\b', // Backspace
    [0x0F] = '\t', // Tab
    [0x10] = 'Q',  [0x11] = 'W',  [0x12] = 'E',  [0x13] = 'R',
    [0x14] = 'T',  [0x15] = 'Y',  [0x16] = 'U',  [0x17] = 'I',
    [0x18] = 'O',  [0x19] = 'P',  [0x1A] = '{',  [0x1B] = '}',
    [0x1C] = '\n', // Enter
    [0x1D] = 0,    // Left Ctrl
    [0x1E] = 'A',  [0x1F] = 'S',  [0x20] = 'D',  [0x21] = 'F',
    [0x22] = 'G',  [0x23] = 'H',  [0x24] = 'J',  [0x25] = 'K',
    [0x26] = 'L',  [0x27] = ':',  [0x28] = '\"', [0x29] = '~',
    [0x2A] = 0,    // Left Shift
    [0x2B] = '|',  [0x2C] = 'Z',  [0x2D] = 'X',  [0x2E] = 'C',
    [0x2F] = 'V',  [0x30] = 'B',  [0x31] = 'N',  [0x32] = 'M',
    [0x33] = '<',  [0x34] = '>',  [0x35] = '?',  [0x36] = 0, // Right Shift
    [0x37] = '*',  // Keypad *
    [0x38] = 0,    // Left Alt
    [0x39] = ' ',  // Space
    [0x3A] = 0,    // CapsLock
    [0x45] = 0,    // NumLock
    [0x47] = 0,    // Home / 7
    [0x48] = 0,    // Up / 8
    [0x49] = 0,    // PgUp / 9
    [0x4A] = '-',  // Keypad -
    [0x4B] = 0,    // Left / 4
    [0x4C] = 0,    // Keypad 5
    [0x4D] = 0,    // Right / 6
    [0x4E] = '+',  // Keypad +
    [0x4F] = 0,    // End / 1
    [0x50] = 0,    // Down / 2
    [0x51] = 0,    // PgDn / 3
    [0x52] = 0,    // Insert / 0
    [0x53] = 0     // Delete / .
};

/* --- Mouse State --- */
static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_z = 0;
static int screen_w = 1024;
static int screen_h = 768;
static bool left_btn = false;
static bool right_btn = false;
static bool middle_btn = false;
static bool prev_left_btn = false;
static bool prev_right_btn = false;

void input_init(void) {
    memset((void *)key_state, 0, sizeof(key_state));
    early_blog(BLOG_OK, "INPUT", "Native Input Core ready");
}

/* Push from USB or PS/2 */
void input_keyboard_push(uint8_t scancode, bool is_release) {
    if (scancode >= 128) return;

    if (is_release) {
        key_state[scancode] = false;
        if (scancode == 0x2A || scancode == 0x36) shift_pressed = false;
        if (scancode == 0x1D) ctrl_pressed = false;
        if (scancode == 0x38) alt_pressed = false;

        int next = (kbd_head + 1) % KBD_BUF_SIZE;
        if (next != kbd_tail) {
            kbd_buffer[kbd_head].c = 0;
            kbd_buffer[kbd_head].scancode = scancode;
            kbd_buffer[kbd_head].is_release = true;
            kbd_buffer[kbd_head].shift = shift_pressed;
            kbd_buffer[kbd_head].ctrl = ctrl_pressed;
            kbd_buffer[kbd_head].alt = alt_pressed;
            kbd_head = next;
        }
    } else {
        key_state[scancode] = true;
        if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
        if (scancode == 0x1D) ctrl_pressed = true;
        if (scancode == 0x38) alt_pressed = true;
        if (scancode == 0x3A) caps_lock = !caps_lock; // Toggle Caps Lock
        if (scancode == 0x45) num_lock = !num_lock;   // Toggle Num Lock

        bool use_shift = shift_pressed;
        char normal_c = scancode_to_char[scancode];
        
        // Handle Num Lock overrides
        if (num_lock && !use_shift) {
            switch(scancode) {
                case 0x47: normal_c = '7'; break;
                case 0x48: normal_c = '8'; break;
                case 0x49: normal_c = '9'; break;
                case 0x4B: normal_c = '4'; break;
                case 0x4C: normal_c = '5'; break;
                case 0x4D: normal_c = '6'; break;
                case 0x4F: normal_c = '1'; break;
                case 0x50: normal_c = '2'; break;
                case 0x51: normal_c = '3'; break;
                case 0x52: normal_c = '0'; break;
                case 0x53: normal_c = '.'; break;
            }
        }
        
        if (caps_lock && normal_c >= 'a' && normal_c <= 'z') {
            use_shift = !use_shift;
        }
        char c = use_shift ? scancode_to_char_shift[scancode] : normal_c;

        int next = (kbd_head + 1) % KBD_BUF_SIZE;
        if (next != kbd_tail) {
            kbd_buffer[kbd_head].c = c;
            kbd_buffer[kbd_head].scancode = scancode;
            kbd_buffer[kbd_head].is_release = false;
            kbd_buffer[kbd_head].shift = shift_pressed;
            kbd_buffer[kbd_head].ctrl = ctrl_pressed;
            kbd_buffer[kbd_head].alt = alt_pressed;
            kbd_head = next;
        }
    }
}

void input_mouse_push(int dx, int dy, int dz, bool left, bool right, bool middle) {
    prev_left_btn = left_btn;
    prev_right_btn = right_btn;
    
    left_btn = left;
    right_btn = right;
    middle_btn = middle;
    
    mouse_x += dx;
    mouse_y -= dy; // Invert Y 
    mouse_z += dz;
    
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= screen_w) mouse_x = screen_w - 1;
    if (mouse_y >= screen_h) mouse_y = screen_h - 1;
}

/* --- Legacy PS/2 & Physical Hardware 8042 Controller --- */
#include "kernel/idt.h"
#include "kernel/irq.h"

static inline void ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 2) == 0) return;
        io_wait();
    }
}

static inline bool ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 1) != 0) return true;
        io_wait();
    }
    return false;
}

static void ps2_kbd_write(uint8_t a_write) {
    ps2_wait_write();
    outb(0x60, a_write);
}

static void ps2_mouse_write(uint8_t a_write) {
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, a_write);
}

static bool kbd_extended = false;
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

static void handle_ps2_scancode(uint8_t scancode) {
    if (scancode == 0xE0) {
        kbd_extended = true;
        return;
    }

    bool is_release = (scancode & 0x80) != 0;
    scancode &= 0x7F;

    if (kbd_extended) {
        kbd_extended = false;
        // Map common extended scancodes to standard keys
        switch (scancode) {
            case 0x48: /* Up */    input_keyboard_push(0x48, is_release); return;
            case 0x50: /* Down */  input_keyboard_push(0x50, is_release); return;
            case 0x4B: /* Left */  input_keyboard_push(0x4B, is_release); return;
            case 0x4D: /* Right */ input_keyboard_push(0x4D, is_release); return;
            case 0x53: /* Delete */ input_keyboard_push(0x53, is_release); return;
            case 0x47: /* Home */  input_keyboard_push(0x47, is_release); return;
            case 0x4F: /* End */   input_keyboard_push(0x4F, is_release); return;
            case 0x49: /* PgUp */  input_keyboard_push(0x49, is_release); return;
            case 0x51: /* PgDn */  input_keyboard_push(0x51, is_release); return;
            default: break;
        }
    }

    input_keyboard_push(scancode, is_release);
}

static void handle_ps2_mouse_byte(uint8_t b) {
    mouse_byte[mouse_cycle] = (int8_t)b;
    if (mouse_cycle == 0 && !(mouse_byte[0] & 0x08)) {
        return; // Bad packet header synchronization, wait for 0x08 bit
    }

    mouse_cycle++;
    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        bool left = mouse_byte[0] & 0x01;
        bool right = mouse_byte[0] & 0x02;
        bool middle = mouse_byte[0] & 0x04;
        
        int dx = mouse_byte[1];
        int dy = mouse_byte[2];
        if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
        if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;
        
        input_mouse_push(dx, dy, 0, left, right, middle);
    }
}

uint64_t ps2_keyboard_handler(struct interrupt_frame *frame) {
    while (1) {
        uint8_t status = inb(0x64);
        if (!(status & 1)) break;

        uint8_t data = inb(0x60);
        if (status & 0x20) {
            // AUX (mouse) packet arrived on keyboard IRQ
            handle_ps2_mouse_byte(data);
        } else {
            handle_ps2_scancode(data);
        }
    }
    return (uint64_t)frame;
}

uint64_t ps2_mouse_handler(struct interrupt_frame *frame) {
    while (1) {
        uint8_t status = inb(0x64);
        if (!(status & 1)) break;

        uint8_t data = inb(0x60);
        if (status & 0x20) {
            handle_ps2_mouse_byte(data);
        } else {
            // Keyboard packet arrived on mouse IRQ
            handle_ps2_scancode(data);
        }
    }
    return (uint64_t)frame;
}

void ps2_fallback_init(void) {
    // 1. Disable devices before configuration
    ps2_wait_write();
    outb(0x64, 0xAD); // Disable keyboard
    ps2_wait_write();
    outb(0x64, 0xA7); // Disable mouse

    // 2. Flush residual buffer
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    // 3. Read Controller Configuration Byte
    ps2_wait_write();
    outb(0x64, 0x20);
    if (ps2_wait_read()) {
        uint8_t conf = inb(0x60);

        // Enable First Port (Keyboard) IRQ, Second Port (Mouse) IRQ, and Translation
        conf |= (1 << 0) | (1 << 1) | (1 << 6);
        // Clear disable clock lines
        conf &= ~((1 << 4) | (1 << 5));

        // Write Configuration Byte back
        ps2_wait_write();
        outb(0x64, 0x60);
        ps2_wait_write();
        outb(0x60, conf);
    }

    // 4. Enable Keyboard Port
    ps2_wait_write();
    outb(0x64, 0xAE);

    // 5. Enable Mouse Port
    ps2_wait_write();
    outb(0x64, 0xA8);

    // 6. Enable Keyboard Scanning (CRITICAL for real hardware like HP laptops!)
    ps2_kbd_write(0xF4);
    if (ps2_wait_read()) {
        inb(0x60); // Consume ACK (0xFA)
    }

    // 7. Enable Mouse Stream
    ps2_mouse_write(0xF6); // Set defaults
    if (ps2_wait_read()) inb(0x60); // Consume ACK

    ps2_mouse_write(0xF4); // Enable packet streaming
    if (ps2_wait_read()) inb(0x60); // Consume ACK

    // 8. Final flush of any lingering bytes
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    register_irq_handler(1, ps2_keyboard_handler);
    register_irq_handler(12, ps2_mouse_handler);
}

/* --- GUI Input API --- */

bool keyboard_has_event(void) {
    return (kbd_head != kbd_tail);
}

int keyboard_poll_event(struct key_event *event) {
    extern void xhci_poll_events(void);
    xhci_poll_events(); // Pump USB events

    // Safety check PS/2 8042 buffer on real laptops
    uint8_t status = inb(0x64);
    if (status & 1) {
        uint8_t data = inb(0x60);
        if (status & 0x20) {
            handle_ps2_mouse_byte(data);
        } else {
            handle_ps2_scancode(data);
        }
    }

    if (kbd_head != kbd_tail) {
        if (event) {
            *event = kbd_buffer[kbd_tail];
        }
        kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
        return 1;
    }
    return 0;
}

int keyboard_poll(char *c) {
    struct key_event ev;
    while (keyboard_poll_event(&ev)) {
        if (!ev.is_release && ev.c != 0) {
            if (c) *c = ev.c;
            return 1;
        }
    }
    return 0;
}

char keyboard_getchar(void) {
    char c = 0;
    while (!keyboard_poll(&c)) {
        extern void task_yield(void);
        task_yield();
    }
    return c;
}

bool keyboard_key_pressed(uint8_t scancode) {
    if (scancode < 128) return key_state[scancode];
    return false;
}

bool keyboard_shift_pressed(void) {
    return shift_pressed;
}

bool keyboard_ctrl_pressed(void) {
    return ctrl_pressed;
}

bool keyboard_alt_pressed(void) {
    return alt_pressed;
}

bool keyboard_caps_locked(void) {
    return caps_lock;
}

int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
int mouse_get_z(void) { return mouse_z; }
bool mouse_left_pressed(void) { return left_btn; }
bool mouse_right_pressed(void) { return right_btn; }
bool mouse_middle_pressed(void) { return middle_btn; }

bool mouse_left_clicked(void) {
    bool clicked = left_btn && !prev_left_btn;
    prev_left_btn = left_btn; 
    return clicked;
}

bool mouse_right_clicked(void) {
    bool clicked = right_btn && !prev_right_btn;
    prev_right_btn = right_btn;
    return clicked;
}

void mouse_set_bounds(int max_x, int max_y) {
    screen_w = max_x;
    screen_h = max_y;
    mouse_x = max_x / 2;
    mouse_y = max_y / 2;
}

void usb_hid_init(void) {
    // Connects to USB Core to parse Input Reports and call push functions
    early_blog(BLOG_OK, "USB", "USB HID Driver initialized");
    
    ps2_fallback_init();
    early_blog(BLOG_OK, "INPUT", "PS/2 Fallback Enabled (for Emulators)");
}
