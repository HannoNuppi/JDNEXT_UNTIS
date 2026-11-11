#ifndef UNTIS_NEXT_UI_H
#define UNTIS_NEXT_UI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t r, g, b, a;
} UiColor;

bool ui_init(void);
void ui_shutdown(void);
bool ui_ready(void);
void ui_begin(void);
void ui_end(void);
void ui_fill(UiColor color);
void ui_rect(int x, int y, int w, int h, UiColor color);
void ui_outline(int x, int y, int w, int h, UiColor color);
void ui_card(int x, int y, int w, int h, bool selected);
void ui_text(int x, int y, const char *text, int scale, UiColor color);
void ui_text_center(int x, int y, int w, const char *text, int scale, UiColor color);
void ui_badge(int x, int y, int w, const char *text, UiColor bg, UiColor fg);
void ui_separator(int x, int y, int w, UiColor color);

#define UI_RGB(r,g,b) ((UiColor){(r),(g),(b),255})
#define UI_BG          UI_RGB(10, 15, 30)
#define UI_SURFACE     UI_RGB(18, 27, 48)
#define UI_SURFACE_2   UI_RGB(25, 37, 63)
#define UI_BORDER      UI_RGB(44, 59, 88)
#define UI_TEXT        UI_RGB(236, 242, 255)
#define UI_MUTED       UI_RGB(145, 161, 190)
#define UI_CYAN        UI_RGB(93, 224, 242)
#define UI_PURPLE      UI_RGB(145, 124, 255)
#define UI_GREEN       UI_RGB(84, 214, 160)
#define UI_YELLOW      UI_RGB(250, 204, 105)
#define UI_RED         UI_RGB(255, 112, 128)

#endif
