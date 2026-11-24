#include "ui_common.h"

#include <math.h>
#include <stdio.h>

#include "ui_hit.h"
#include "util.h"

#define BATTERY_LOW 15

static void drawBattery(int x, int y, int percent, bool charging, Color text, Color dim)
{
    Color fill = charging ? COLOR_CHARGING : percent <= BATTERY_LOW ? COLOR_ERROR : text;
    gfxFillRoundRect(x, y, 30, 16, 4, dim);
    gfxFillRoundRect(x + 2, y + 2, 26, 12, 3, COLOR_BG);
    gfxFillRoundRect(x + 31, y + 5, 3, 6, 1, dim);
    int inner = 22 * (percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100;
    if (inner > 0)
        gfxFillRoundRect(x + 4, y + 4, inner, 8, 2, fill);
    if (charging) {  // A lightning bolt over the battery.
        float cx = (float)x + 15.0f, cy = (float)y + 8.0f;
        gfxTriangle(cx + 2.0f, cy - 8.0f, cx - 5.0f, cy + 1.5f, cx + 1.5f, cy + 1.5f, COLOR_BG);
        gfxTriangle(cx - 2.0f, cy + 8.0f, cx + 5.0f, cy - 1.5f, cx - 1.5f, cy - 1.5f, COLOR_BG);
        gfxTriangle(cx + 1.5f, cy - 6.0f, cx - 3.5f, cy + 1.0f, cx + 1.0f, cy + 1.0f, COLOR_TEXT);
        gfxTriangle(cx - 1.5f, cy + 6.0f, cx + 3.5f, cy - 1.0f, cx - 1.0f, cy - 1.0f, COLOR_TEXT);
    }
}

void uiDrawStatus(const SysInfo *sys, bool clock_24h, int right, int y, Color text, Color dim)
{
    int x = right;
    if (sys->has_battery) {
        char percent[8];
        snprintf(percent, sizeof(percent), "%d%%", sys->battery_percent);
        x -= 34;
        drawBattery(x, y + 2, sys->battery_percent, sys->charging, text, dim);
        x -= fontTextWidth(FONT_REGULAR, 17, percent) + 8;
        fontDraw(FONT_REGULAR, 17, x, y + 1, percent, sys->charging ? COLOR_CHARGING : dim);
        x -= 20;
    }
    if (sys->has_time) {
        char clock[16];
        utilFormatClock(clock, sizeof(clock), sys->hour, sys->minute, clock_24h);
        x -= fontTextWidth(FONT_BOLD, 18, clock);
        fontDraw(FONT_BOLD, 18, x, y, clock, text);
    }
}

void uiDrawHeader(const char *title)
{
    float cx = MARGIN + 10.0f, cy = 32.0f;
    gfxLine(cx - 8, cy - 8, cx + 8, cy + 8, 3.0f, COLOR_DIM);
    gfxLine(cx - 8, cy + 8, cx + 8, cy - 8, 3.0f, COLOR_DIM);
    uiHitAdd(0, 0, MARGIN + 34, HEADER_H, HIT_EXIT, 0);
    int title_w = fontDraw(FONT_BOLD, 26, MARGIN + 34, 16, title, COLOR_ACCENT);
    uiHitAdd(MARGIN + 34, 0, title_w + 12, HEADER_H, HIT_MENU_BUTTON, 0);  // As + does: the menu.
}

void uiDrawSpinner(float cx, float cy, float r, uint64_t now_ns, Color color)
{
    const int dots = 8;
    double turn = (double)(now_ns % 1000000000ull) / 1e9;
    for (int i = 0; i < dots; i++) {
        double a = (double)i / dots * 2 * M_PI;
        double fade = fmod(turn - (double)i / dots + 1.0, 1.0);
        unsigned alpha = (unsigned)(60 + 195 * (1.0 - fade));
        int x = (int)lround(cx + cos(a) * r), y = (int)lround(cy + sin(a) * r);
        int d = (int)(r * 0.32f);
        gfxBlendRect(x - d / 2, y - d / 2, d, d, color, alpha);
    }
}
