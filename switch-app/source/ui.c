#include "ui.h"
#include <switch.h>
#include <string.h>

#define UI_WIDTH 1280
#define UI_HEIGHT 720

static Framebuffer g_framebuffer;
static u32 *g_pixels = NULL;
static u32 g_stride_pixels = 0;
static bool g_ready = false;

static UiColor g_theme_bg = UI_RGB(10, 15, 30);
static UiColor g_theme_surface = UI_RGB(18, 27, 48);
static UiColor g_theme_surface_2 = UI_RGB(25, 37, 63);
static UiColor g_theme_border = UI_RGB(44, 59, 88);
static UiColor g_theme_text = UI_RGB(236, 242, 255);
static UiColor g_theme_muted = UI_RGB(145, 161, 190);
static UiColor g_theme_cyan = UI_RGB(93, 224, 242);
static UiColor g_theme_purple = UI_RGB(145, 124, 255);
static UiColor g_theme_green = UI_RGB(84, 214, 160);
static UiColor g_theme_yellow = UI_RGB(250, 204, 105);
static UiColor g_theme_red = UI_RGB(255, 112, 128);


void ui_set_theme(const char *theme_id) {
    g_theme_bg = UI_RGB(10, 15, 30);
    g_theme_surface = UI_RGB(18, 27, 48);
    g_theme_surface_2 = UI_RGB(25, 37, 63);
    g_theme_border = UI_RGB(44, 59, 88);
    g_theme_text = UI_RGB(236, 242, 255);
    g_theme_muted = UI_RGB(145, 161, 190);
    g_theme_cyan = UI_RGB(93, 224, 242);
    g_theme_purple = UI_RGB(145, 124, 255);
    g_theme_green = UI_RGB(84, 214, 160);
    g_theme_yellow = UI_RGB(250, 204, 105);
    g_theme_red = UI_RGB(255, 112, 128);

    if (!theme_id) return;
    if (strcmp(theme_id, "Ocean") == 0) {
        g_theme_bg=UI_RGB(5,17,32); g_theme_surface=UI_RGB(7,35,55);
        g_theme_surface_2=UI_RGB(10,61,82); g_theme_border=UI_RGB(28,102,124);
        g_theme_cyan=UI_RGB(66,223,255); g_theme_purple=UI_RGB(67,108,255);
        g_theme_green=UI_RGB(125,255,240); g_theme_yellow=UI_RGB(247,227,125);
    } else if (strcmp(theme_id, "Sunset") == 0) {
        g_theme_bg=UI_RGB(34,12,27); g_theme_surface=UI_RGB(55,21,42);
        g_theme_surface_2=UI_RGB(88,31,57); g_theme_border=UI_RGB(133,61,77);
        g_theme_cyan=UI_RGB(255,195,110); g_theme_purple=UI_RGB(155,77,190);
        g_theme_green=UI_RGB(255,170,133); g_theme_yellow=UI_RGB(255,223,132);
    } else if (strcmp(theme_id, "Forest") == 0) {
        g_theme_bg=UI_RGB(5,24,21); g_theme_surface=UI_RGB(12,48,39);
        g_theme_surface_2=UI_RGB(20,75,62); g_theme_border=UI_RGB(37,110,87);
        g_theme_cyan=UI_RGB(99,241,210); g_theme_purple=UI_RGB(55,126,118);
        g_theme_green=UI_RGB(134,225,124); g_theme_yellow=UI_RGB(226,227,109);
    } else if (strcmp(theme_id, "Candy") == 0) {
        g_theme_bg=UI_RGB(33,10,45); g_theme_surface=UI_RGB(61,19,68);
        g_theme_surface_2=UI_RGB(94,31,86); g_theme_border=UI_RGB(135,53,115);
        g_theme_cyan=UI_RGB(131,234,255); g_theme_purple=UI_RGB(179,92,255);
        g_theme_green=UI_RGB(131,255,212); g_theme_yellow=UI_RGB(255,227,127);
    } else if (strcmp(theme_id, "Midnight") == 0) {
        g_theme_bg=UI_RGB(5,8,20); g_theme_surface=UI_RGB(16,20,39);
        g_theme_surface_2=UI_RGB(35,34,59); g_theme_border=UI_RGB(64,65,99);
        g_theme_cyan=UI_RGB(120,170,255); g_theme_purple=UI_RGB(108,93,232);
        g_theme_green=UI_RGB(142,180,255); g_theme_yellow=UI_RGB(212,222,255);
    } else if (strcmp(theme_id, "Aurora") == 0) {
        g_theme_bg=UI_RGB(6,23,48); g_theme_surface=UI_RGB(22,26,72);
        g_theme_surface_2=UI_RGB(31,67,85); g_theme_border=UI_RGB(60,100,128);
        g_theme_cyan=UI_RGB(107,255,228); g_theme_purple=UI_RGB(115,111,255);
        g_theme_green=UI_RGB(140,255,205); g_theme_yellow=UI_RGB(255,239,154);
    } else if (strcmp(theme_id, "Retro") == 0) {
        g_theme_bg=UI_RGB(29,17,40); g_theme_surface=UI_RGB(57,32,69);
        g_theme_surface_2=UI_RGB(97,54,80); g_theme_border=UI_RGB(140,75,107);
        g_theme_cyan=UI_RGB(112,225,194); g_theme_purple=UI_RGB(196,90,162);
        g_theme_green=UI_RGB(135,225,160); g_theme_yellow=UI_RGB(244,211,94);
    } else if (strcmp(theme_id, "RoyalRebels") == 0) {
        g_theme_bg=UI_RGB(19,10,30); g_theme_surface=UI_RGB(49,21,56);
        g_theme_surface_2=UI_RGB(22,49,87); g_theme_border=UI_RGB(94,49,110);
        g_theme_cyan=UI_RGB(112,223,255); g_theme_purple=UI_RGB(155,82,201);
        g_theme_green=UI_RGB(130,224,190); g_theme_yellow=UI_RGB(230,184,255);
    } else if (strcmp(theme_id, "Bronze") == 0) {
        g_theme_bg=UI_RGB(28,17,10); g_theme_surface=UI_RGB(65,39,22);
        g_theme_surface_2=UI_RGB(104,65,37); g_theme_border=UI_RGB(149,94,58);
        g_theme_cyan=UI_RGB(217,179,143); g_theme_purple=UI_RGB(153,97,63);
        g_theme_green=UI_RGB(191,173,126); g_theme_yellow=UI_RGB(239,191,123);
    } else if (strcmp(theme_id, "Silver") == 0) {
        g_theme_bg=UI_RGB(14,21,33); g_theme_surface=UI_RGB(43,53,72);
        g_theme_surface_2=UI_RGB(75,91,116); g_theme_border=UI_RGB(122,140,165);
        g_theme_cyan=UI_RGB(154,233,255); g_theme_purple=UI_RGB(145,166,216);
        g_theme_green=UI_RGB(186,221,224); g_theme_yellow=UI_RGB(240,244,255);
    } else if (strcmp(theme_id, "Gold") == 0 || strcmp(theme_id, "WinterGoldEvent") == 0) {
        g_theme_bg=UI_RGB(31,22,7); g_theme_surface=UI_RGB(72,51,13);
        g_theme_surface_2=UI_RGB(124,94,25); g_theme_border=UI_RGB(173,136,52);
        g_theme_cyan=UI_RGB(255,240,166); g_theme_purple=UI_RGB(219,166,75);
        g_theme_green=UI_RGB(208,225,165); g_theme_yellow=UI_RGB(255,240,166);
    } else if (strcmp(theme_id, "WinterEvent") == 0) {
        g_theme_bg=UI_RGB(5,20,42); g_theme_surface=UI_RGB(15,48,78);
        g_theme_surface_2=UI_RGB(29,81,117); g_theme_border=UI_RGB(90,156,195);
        g_theme_cyan=UI_RGB(230,251,255); g_theme_purple=UI_RGB(141,188,245);
        g_theme_green=UI_RGB(188,234,255); g_theme_yellow=UI_RGB(255,231,163);
    } else if (strcmp(theme_id, "BirthdayEvent") == 0) {
        g_theme_bg=UI_RGB(41,18,48); g_theme_surface=UI_RGB(94,43,76);
        g_theme_surface_2=UI_RGB(144,73,115); g_theme_border=UI_RGB(200,121,148);
        g_theme_cyan=UI_RGB(157,247,237); g_theme_purple=UI_RGB(198,139,255);
        g_theme_green=UI_RGB(185,240,193); g_theme_yellow=UI_RGB(255,225,160);
    } else if (strcmp(theme_id, "HalloweenEvent") == 0) {
        g_theme_bg=UI_RGB(20,8,29); g_theme_surface=UI_RGB(53,17,67);
        g_theme_surface_2=UI_RGB(92,31,45); g_theme_border=UI_RGB(147,65,39);
        g_theme_cyan=UI_RGB(214,160,255); g_theme_purple=UI_RGB(152,73,196);
        g_theme_green=UI_RGB(178,226,144); g_theme_yellow=UI_RGB(255,193,107);
    } else if (strcmp(theme_id, "AdminMatrix") == 0) {
        g_theme_bg=UI_RGB(1,6,4); g_theme_surface=UI_RGB(2,20,10);
        g_theme_surface_2=UI_RGB(4,54,23); g_theme_border=UI_RGB(18,119,55);
        g_theme_cyan=UI_RGB(184,255,202); g_theme_purple=UI_RGB(18,184,78);
        g_theme_green=UI_RGB(72,255,131); g_theme_yellow=UI_RGB(228,255,233);
    }
}

UiColor ui_color_bg(void) { return g_theme_bg; }
UiColor ui_color_surface(void) { return g_theme_surface; }
UiColor ui_color_surface_2(void) { return g_theme_surface_2; }
UiColor ui_color_border(void) { return g_theme_border; }
UiColor ui_color_text(void) { return g_theme_text; }
UiColor ui_color_muted(void) { return g_theme_muted; }
UiColor ui_color_cyan(void) { return g_theme_cyan; }
UiColor ui_color_purple(void) { return g_theme_purple; }
UiColor ui_color_green(void) { return g_theme_green; }
UiColor ui_color_yellow(void) { return g_theme_yellow; }
UiColor ui_color_red(void) { return g_theme_red; }

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
