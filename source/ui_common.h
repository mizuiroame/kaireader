// Colors, layout and small drawings shared by KaiReader's screens.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "font.h"
#include "gfx.h"
#include "theme.h"
#include "sysinfo.h"

#define COLOR_BG GFX_RGB(0x12, 0x12, 0x14)
#define COLOR_SURFACE GFX_RGB(0x1C, 0x1C, 0x21)
#define COLOR_SURFACE_HI GFX_RGB(0x2C, 0x2C, 0x34)
// KaiReader's color: the warm light of a reading lamp.
#define COLOR_ACCENT g_accent  // Chosen in Settings (theme.h).
#define COLOR_TEXT GFX_RGB(0xF2, 0xF2, 0xF5)
#define COLOR_DIM GFX_RGB(0xA4, 0xA4, 0xB0)
#define COLOR_FAINT GFX_RGB(0x5E, 0x5E, 0x6A)
#define COLOR_ERROR GFX_RGB(0xFF, 0x8A, 0x5C)
#define COLOR_NETWORK g_network_color  // The network folder's, as in KaiPlayer and KaiVideo.
#define COLOR_SERVER g_server_color   // The book server's: the deep blue of KaiPlayer's server.
#define COLOR_NEWS g_news_color       // The news': coral.
#define COLOR_FREE GFX_RGB(0x5B, 0xD6, 0x7A)     // Free books': green.
#define COLOR_CHARGING GFX_RGB(0x5B, 0xD6, 0x7A)

#define MARGIN 32
#define HEADER_H 64
#define UI_SLIDER_LEFT (MARGIN + 140)  // The reader's page slider: from here to as far from the right.

// A 0xRRGGBB color (a page's paper or ink) as a Color.
static inline Color uiColorOf(uint32_t rgb)
{
    return GFX_RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

// The clock and the battery, right-aligned to right at y (the header's line).
// Where they begin, at the left.
int uiDrawStatus(const SysInfo *sys, bool clock_24h, int right, int y, Color text, Color dim);

// The X (quits) and the title (opens the menu) at the top left.
void uiDrawHeader(const char *title);

// A five-pointed star (a favorite) of outer radius r around (cx, cy).
void uiDrawStar(float cx, float cy, float r, Color color);
// On the card (a free book downloaded already), as KaiVideo marks a download: an SD card
// in a filled circle of the accent, radius r around (cx, cy).
void uiDrawOnCard(float cx, float cy, float r);
// Splits text into up to max_lines lines of width w (words kept whole when they can be): how many (ui.c).
int uiWrapLines(FontFace face, int px, const char *text, int w, char lines[][160], int max_lines);
// A spinner: dots around (cx, cy) going round.
void uiDrawSpinner(float cx, float cy, float r, uint64_t now_ns, Color color);
