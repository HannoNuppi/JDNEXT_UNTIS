#include "ui.h"

#include <SDL.h>
#include <ctype.h>
#include <string.h>

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static bool g_ready = false;

static void set_color(UiColor color) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, color.r, color.g, color.b, color.a);
}

/* Compact 5x7 uppercase bitmap font; no external font or browser is needed. */
static void glyph(char c, uint8_t out[5]) {
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
        {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
    };
    static const uint8_t digits[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
        {0x62,0x51,0x49,0x49,0x46}, {0x22,0x49,0x49,0x49,0x36},
        {0x18,0x14,0x12,0x7F,0x10}, {0x2F,0x49,0x49,0x49,0x31},
        {0x3E,0x49,0x49,0x49,0x32}, {0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36}, {0x26,0x49,0x49,0x49,0x3E}
    };

    memset(out, 0, 5);
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') {
        memcpy(out, letters[c - 'A'], 5);
        return;
    }
    if (c >= '0' && c <= '9') {
        memcpy(out, digits[c - '0'], 5);
        return;
    }
    switch (c) {
        case ' ': break;
        case '.': out[2]=0x60; break;
        case ',': out[2]=0x60; out[3]=0x20; break;
        case ':': out[2]=0x36; break;
        case ';': out[2]=0x56; out[3]=0x20; break;
        case '-': out[1]=0x08; out[2]=0x08; out[3]=0x08; break;
        case '_': out[0]=0x40; out[1]=0x40; out[2]=0x40; out[3]=0x40; out[4]=0x40; break;
        case '+': out[1]=0x08; out[2]=0x3E; out[3]=0x08; break;
        case '/': out[0]=0x60; out[1]=0x18; out[2]=0x06; out[3]=0x01; break;
        case '\\': out[0]=0x01; out[1]=0x06; out[2]=0x18; out[3]=0x60; break;
        case '!': out[2]=0x5F; break;
        case '?': out[0]=0x02; out[1]=0x01; out[2]=0x59; out[3]=0x09; out[4]=0x06; break;
        case 39: out[2]=0x07; break;
        case '"': out[1]=0x07; out[3]=0x07; break;
        case '(': out[1]=0x1C; out[2]=0x22; out[3]=0x41; break;
        case ')': out[1]=0x41; out[2]=0x22; out[3]=0x1C; break;
        case '[': out[1]=0x7F; out[2]=0x41; out[3]=0x41; break;
        case ']': out[1]=0x41; out[2]=0x41; out[3]=0x7F; break;
        case '#': out[0]=0x14; out[1]=0x7F; out[2]=0x14; out[3]=0x7F; out[4]=0x14; break;
        case '*': out[1]=0x14; out[2]=0x08; out[3]=0x14; break;
        case '=': out[1]=0x14; out[2]=0x14; out[3]=0x14; break;
        case '<': out[1]=0x08; out[2]=0x14; out[3]=0x22; break;
        case '>': out[1]=0x22; out[2]=0x14; out[3]=0x08; break;
        case '%': out[0]=0x63; out[1]=0x13; out[2]=0x08; out[3]=0x64; out[4]=0x63; break;
        default: out[0]=0x02; out[1]=0x01; out[2]=0x51; out[3]=0x09; out[4]=0x06; break;
    }
}

bool ui_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    g_window = SDL_CreateWindow("Untis Next", 0, 0, 1280, 720, 0);
    if (!g_window) {
        SDL_Quit();
        return false;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        SDL_Quit();
        return false;
    }
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    g_ready = true;
    return true;
}

void ui_shutdown(void) {
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_renderer = NULL;
    g_window = NULL;
    if (g_ready) SDL_Quit();
    g_ready = false;
}

bool ui_ready(void) { return g_ready; }

void ui_begin(void) {
    if (!g_ready) return;
    set_color(UI_BG);
    SDL_RenderClear(g_renderer);
}

void ui_end(void) {
    if (g_ready) SDL_RenderPresent(g_renderer);
}

void ui_fill(UiColor color) {
    if (!g_ready) return;
    set_color(color);
    SDL_Rect r = {0, 0, 1280, 720};
    SDL_RenderFillRect(g_renderer, &r);
}

void ui_rect(int x, int y, int w, int h, UiColor color) {
    if (!g_ready || w <= 0 || h <= 0) return;
    set_color(color);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(g_renderer, &r);
}

void ui_outline(int x, int y, int w, int h, UiColor color) {
    if (!g_ready || w <= 0 || h <= 0) return;
    set_color(color);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderDrawRect(g_renderer, &r);
}

void ui_card(int x, int y, int w, int h, bool selected) {
    ui_rect(x, y, w, h, selected ? UI_SURFACE_2 : UI_SURFACE);
    ui_outline(x, y, w, h, selected ? UI_CYAN : UI_BORDER);
    if (selected) ui_rect(x, y, 4, h, UI_CYAN);
}

static void draw_utf8_text(int x, int y, const char *text, int scale, UiColor color) {
    if (!g_ready || !text || scale < 1) return;
    int cursor = x;
    set_color(color);

    for (size_t i = 0; text[i];) {
        unsigned char c = (unsigned char)text[i++];
        char mapped[3] = {0,0,0};
        if (c >= 0xC0) {
            unsigned char d = (unsigned char)text[i];
            if (d) i++;
            if (c == 0xC3) {
                if (d == 0x84 || d == 0xA4) mapped[0] = 'A';
                else if (d == 0x96 || d == 0xB6) mapped[0] = 'O';
                else if (d == 0x9C || d == 0xBC) mapped[0] = 'U';
                else if (d == 0x9F) { mapped[0] = 'S'; mapped[1] = 'S'; }
                else mapped[0] = '?';
            } else mapped[0] = '?';
        } else {
            mapped[0] = (char)c;
        }

        for (int m = 0; mapped[m]; m++) {
            uint8_t columns[5];
            glyph(mapped[m], columns);
            for (int col = 0; col < 5; col++) {
                for (int row = 0; row < 7; row++) {
                    if (columns[col] & (1u << row)) {
                        SDL_Rect pixel = {cursor + col * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(g_renderer, &pixel);
                    }
                }
            }
            cursor += 6 * scale;
        }
    }
}

void ui_text(int x, int y, const char *text, int scale, UiColor color) {
    draw_utf8_text(x, y, text, scale, color);
}

void ui_text_center(int x, int y, int w, const char *text, int scale, UiColor color) {
    if (!text || scale < 1) return;
    int chars = 0;
    for (size_t i = 0; text[i]; i++) if (((unsigned char)text[i] & 0xC0) != 0x80) chars++;
    int text_w = chars * 6 * scale;
    int x0 = x + (w - text_w) / 2;
    if (x0 < x) x0 = x;
    ui_text(x0, y, text, scale, color);
}

void ui_badge(int x, int y, int w, const char *text, UiColor bg, UiColor fg) {
    ui_rect(x, y, w, 26, bg);
    ui_text_center(x + 4, y + 8, w - 8, text, 1, fg);
}

void ui_separator(int x, int y, int w, UiColor color) {
    ui_rect(x, y, w, 1, color);
}
