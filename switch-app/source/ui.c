#include "ui.h"
#include <switch.h>
#include <string.h>

#define UI_WIDTH 1280
#define UI_HEIGHT 720

static Framebuffer g_framebuffer;
static u32 *g_pixels = NULL;
static u32 g_stride_pixels = 0;
static bool g_ready = false;

static u32 pack_color(UiColor c) {
    return RGBA8(c.r, c.g, c.b, c.a);
}

bool ui_init(void) {
    memset(&g_framebuffer, 0, sizeof(g_framebuffer));
    Result rc = framebufferCreate(&g_framebuffer, nwindowGetDefault(),
        UI_WIDTH, UI_HEIGHT, PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(rc)) return false;

    rc = framebufferMakeLinear(&g_framebuffer);
    if (R_FAILED(rc)) {
        framebufferClose(&g_framebuffer);
        return false;
    }
    g_ready = true;
    return true;
}

void ui_shutdown(void) {
    g_pixels = NULL;
    g_stride_pixels = 0;
    if (g_framebuffer.has_init) framebufferClose(&g_framebuffer);
    g_ready = false;
}

bool ui_ready(void) { return g_ready; }

void ui_begin(void) {
    if (!g_ready) return;
    u32 stride_bytes = 0;
    g_pixels = (u32 *)framebufferBegin(&g_framebuffer, &stride_bytes);
    g_stride_pixels = stride_bytes / sizeof(u32);
    if (!g_pixels || !g_stride_pixels) return;
    memset(g_pixels, 0, stride_bytes * UI_HEIGHT);
    ui_fill(UI_BG);
}

void ui_end(void) {
    if (!g_ready || !g_pixels) return;
    framebufferEnd(&g_framebuffer);
    g_pixels = NULL;
}

void ui_rect(int x, int y, int w, int h, UiColor color) {
    if (!g_pixels || w <= 0 || h <= 0) return;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = x + w > UI_WIDTH ? UI_WIDTH : x + w;
    int bottom = y + h > UI_HEIGHT ? UI_HEIGHT : y + h;
    if (right <= left || bottom <= top) return;
    const u32 value = pack_color(color);
    for (int row = top; row < bottom; row++) {
        u32 *dst = g_pixels + (size_t)row * g_stride_pixels + left;
        for (int col = left; col < right; col++) *dst++ = value;
    }
}

void ui_fill(UiColor color) {
    ui_rect(0, 0, UI_WIDTH, UI_HEIGHT, color);
}

void ui_outline(int x, int y, int w, int h, UiColor color) {
    if (w <= 0 || h <= 0) return;
    ui_rect(x, y, w, 1, color);
    ui_rect(x, y + h - 1, w, 1, color);
    ui_rect(x, y, 1, h, color);
    ui_rect(x + w - 1, y, 1, h, color);
}

void ui_card(int x, int y, int w, int h, bool selected) {
    ui_rect(x, y, w, h, selected ? UI_SURFACE_2 : UI_SURFACE);
    ui_outline(x, y, w, h, selected ? UI_CYAN : UI_BORDER);
    if (selected) ui_rect(x, y, 4, h, UI_CYAN);
}

/* Compact 5x7 bitmap font; entirely software-rendered into the native framebuffer. */
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
    if (c >= 'A' && c <= 'Z') { memcpy(out, letters[c - 'A'], 5); return; }
    if (c >= '0' && c <= '9') { memcpy(out, digits[c - '0'], 5); return; }
    switch (c) {
        case ' ': break;
        case '.': out[2]=0x60; break;
        case ',': out[2]=0x60; out[3]=0x20; break;
        case ':': out[2]=0x36; break;
        case ';': out[2]=0x56; out[3]=0x20; break;
        case '-': out[1]=0x08; out[2]=0x08; out[3]=0x08; break;
        case '_': for (int i=0;i<5;i++) out[i]=0x40; break;
        case '+': out[1]=0x08; out[2]=0x3E; out[3]=0x08; break;
        case '/': out[0]=0x60; out[1]=0x18; out[2]=0x06; out[3]=0x01; break;
        case 92: out[0]=0x01; out[1]=0x06; out[2]=0x18; out[3]=0x60; break;
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

void ui_text(int x, int y, const char *text, int scale, UiColor color) {
    if (!g_pixels || !text || scale < 1) return;
    int cursor = x;
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
        } else mapped[0] = (char)c;

        for (int m = 0; mapped[m]; m++) {
            uint8_t columns[5];
            glyph(mapped[m], columns);
            for (int col = 0; col < 5; col++) {
                for (int row = 0; row < 7; row++) {
                    if (columns[col] & (1u << row))
                        ui_rect(cursor + col * scale, y + row * scale, scale, scale, color);
                }
            }
            cursor += 6 * scale;
        }
    }
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
