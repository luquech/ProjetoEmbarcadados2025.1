#ifndef DISPLAY_H
#define DISPLAY_H

#include <driver/i2c.h>
#include <stdbool.h>
#include <stdint.h>

// Definições do display OLED
#define OLED_I2C_ADDRESS 0x3C
#define OLED_CONTROL_BYTE_CMD_STREAM 0x00
#define OLED_CONTROL_BYTE_DATA_STREAM 0x40

// Comandos do display
#define OLED_CMD_SET_CHARGE_PUMP 0x8D
#define OLED_CMD_SET_SEGMENT_REMAP 0xA1
#define OLED_CMD_SET_COM_SCAN_MODE 0xC8
#define OLED_CMD_DISPLAY_ON 0xAF
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20
#define OLED_CMD_SET_COLUMN_RANGE 0x21
#define OLED_CMD_SET_PAGE_RANGE 0x22

// Configurações do display
#define WIDTH 128
#define HEIGHT 64
#define BUFFER_SIZE (WIDTH * HEIGHT / 8)

extern uint8_t display_buffer[BUFFER_SIZE];

void i2c_master_init();
void ssd1306_init();
void clear_screen();
void update_display();
void draw_pixel(int x, int y, bool on);
void draw_rect(int x, int y, int width, int height, bool fill);
void draw_char(int x, int y, char c);
void draw_text(int x, int y, const char *text);

#endif // DISPLAY_H
