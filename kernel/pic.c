/*
 * Zarqaa OS 0.1 - PIC (Programmable Interrupt Controller) Driver
 * Creator: Abdelhak Chabbi
 */

#include "kernel/pic.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10
#define ICW4_8086       0x01

void pic_init(void) {
    /* Start initialization sequence (in cascade mode) */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Master PIC vector offset */
    outb(PIC1_DATA, 0x20); // IRQ0-7 map to 32-39
    io_wait();
    /* ICW2: Slave PIC vector offset */
    outb(PIC2_DATA, 0x28); // IRQ8-15 map to 40-47
    io_wait();

    /* ICW3: tell Master PIC that there is a slave PIC at IRQ2 */
    outb(PIC1_DATA, 4);
    io_wait();
    /* ICW3: tell Slave PIC its cascade identity */
    outb(PIC2_DATA, 2);
    io_wait();

    /* ICW4: have the PICs use 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore saved masks (or mask all) */
    pic_mask_all();

    /* Enable essential IRQs: Timer(0), Keyboard(1), Cascade(2), Mouse(12) */
    pic_unmask(0);
    pic_unmask(1);
    pic_unmask(2);
    pic_unmask(12);
}

void pic_mask_all(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_unmask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_eoi(uint8_t irq) {
    if(irq >= 8) {
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
}
