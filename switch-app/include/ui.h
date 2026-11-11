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
void ui_set_theme(const char *theme_id);
UiColor ui_color_bg(void);
UiColor ui_color_surface(void);
UiColor ui_color_surface_2(void);
UiColor ui_color_border(void);
UiColor ui_color_text(void);
UiColor ui_color_muted(void);
UiColor ui_color_cyan(void);
UiColor ui_color_purple(void);
UiColor ui_color_green(void);
UiColor ui_color_yellow(void);
UiColor ui_color_red(void);
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
#define UI_BG          ui_color_bg()
#define UI_SURFACE     ui_color_surface()
#define UI_SURFACE_2   ui_color_surface_2()
#define UI_BORDER      ui_color_border()
#define UI_TEXT        ui_color_text()
#define UI_MUTED       ui_color_muted()
#define UI_CYAN        ui_color_cyan()
#define UI_PURPLE      ui_color_purple()
#define UI_GREEN       ui_color_green()
#define UI_YELLOW      ui_color_yellow()
#define UI_RED         ui_color_red()

#endif
