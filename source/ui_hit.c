#include "ui_hit.h"

#include <stddef.h>

static HitRegion g_regions[HIT_MAX_REGIONS];
static int g_count;

void uiHitReset(void)
{
    g_count = 0;
}

static int g_clip_top = -100000, g_clip_bottom = 100000;

void uiHitClip(int top, int bottom)
{
    g_clip_top = top;
    g_clip_bottom = bottom;
}

void uiHitAdd(int x, int y, int w, int h, HitKind kind, int arg)
{
    int y0 = y < g_clip_top ? g_clip_top : y, y1 = y + h > g_clip_bottom ? g_clip_bottom : y + h;
    y = y0;
    h = y1 - y0;
    if (g_count < HIT_MAX_REGIONS && w > 0 && h > 0)
        g_regions[g_count++] = (HitRegion){x, y, w, h, kind, arg};
}

int uiHitCount(void)
{
    return g_count;
}

const HitRegion *uiHitAt(int index)
{
    return index >= 0 && index < g_count ? &g_regions[index] : NULL;
}

const HitRegion *uiHitFind(int x, int y)
{
    for (int i = g_count - 1; i >= 0; i--) {  // Added later = drawn on top.
        const HitRegion *r = &g_regions[i];
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h)
            return r;
    }
    return NULL;
}
