
#ifndef SH1106_H
#define SH1106_H

// Kijelző mérete
#define WIDTH 128
#define HEIGHT 64

#define SH1106_ADDR     0x3C

void sh1106_write_cmd(uint8_t cmd);
void sh1106_write_data(uint8_t *data, size_t len);
void sh1106_init();
void sh1106_set_pixel(int x, int y);
void sh1106_reset_pixel(int x, int y);
void draw_block(int x, int y, int size);
void draw_bytes(uint8_t byte1, uint8_t byte2, int start_x, int start_y);
void sh1106_update();
void sh1106_clear();
void draw_text(const char *text, int x, int y);
void rotate_font();

static uint8_t console_font_8x8[];

#endif /* _FONT_8X8_H */
/* end of font_8x8.h */