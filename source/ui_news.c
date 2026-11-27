// The news as a front page, as KaiVideo's Showcase with landscape tiles: the
// newest story large across the top (its picture at the right, its feed, its
// headline, when, and its first words beside it), then rows of wide cards,
// each its picture (16:9, filled) or, without one, a card in its feed's color
// with its feed's name and first words; under it the headline, the feed and
// when. A dot marks the ones not read yet.
#include "ui_news.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "covers.h"
#include "feeds.h"
#include "font.h"
#include "lang.h"
#include "ui_common.h"
#include "ui_hit.h"

#define LEAD_UPRIGHT_TEXT 230  // Upright: the room for its words under its picture.
#define TEXT_LINE_MAX 160

static const Color FEED_COLORS[] = {
    GFX_RGB(0x3D, 0x5A, 0xA8), GFX_RGB(0x2E, 0x7D, 0x6B), GFX_RGB(0x9C, 0x3D, 0x5A),
    GFX_RGB(0xA8, 0x6A, 0x2E), GFX_RGB(0x5E, 0x3D, 0x9C), GFX_RGB(0x4A, 0x6B, 0x2E),
};

static float fb(float v)
{
    return v * GFX_SCALE;
}

#define CARD_PICTURE_MAX 960   // Pixels across a card's picture is kept at, at most,
#define LEAD_PICTURE_MAX 1280  // and the lead story's.

// The pixels a picture drawn w layout units wide needs (asked in a square: one taller than 16:9
// still covers its card when cut).
static int pictureSize(int w, int max)
{
    int px = (int)(fb(w) * gfxDetail() + 0.5f);
    return px > max ? max : px;
}

void uiNewsCoverSize(int *w, int *h)
{
    int card, ph;
    appNewsCardSize(&card, &ph);
    *w = *h = pictureSize(card, CARD_PICTURE_MAX);
}

// A feed's color: the same for every article of it (its hash, in the article's path).
static Color feedColor(const ShelfBook *b)
{
    unsigned h = 5381;
    const char *slash = strchr(b->path, '/');
    for (const char *s = b->path; *s && s != slash; s++)
        h = h * 33 + (unsigned char)*s;
    return FEED_COLORS[h % (sizeof(FEED_COLORS) / sizeof(FEED_COLORS[0]))];
}

void uiNewsWhen(const ShelfBook *b, char *out, size_t size)
{
    int64_t t = feedsParseDate(b->genre), ago = (int64_t)time(NULL) - t;
    if (!t) {
        snprintf(out, size, "%s", "");
    } else if (ago < 0 || ago >= 2 * 86400) {
        snprintf(out, size, "%.10s", b->genre);  // Its day.
    } else if (ago < 60) {
        snprintf(out, size, "%s", T("Just now"));
    } else if (ago < 3600) {
        snprintf(out, size, T("%d min ago"), (int)(ago / 60));
    } else if (ago < 86400) {
        snprintf(out, size, T("%d h ago"), (int)(ago / 3600));
    } else {
        snprintf(out, size, "%s", T("Yesterday"));
    }
}

// The view's clip and a box's, both: what's drawn in the box stays in the library's view.
static void clipTo(int x, int y, int w, int h, int view_top, int view_bottom)
{
    int top = y > view_top ? y : view_top, bottom = y + h < view_bottom ? y + h : view_bottom;
    gfxSetClip(x, top, w, bottom > top ? bottom - top : 0);
}

// The article's picture filling w x h (cut at the sides or top and bottom), or false if it has none (yet).
static bool drawPicture(App *app, const ShelfBook *b, int x, int y, int w, int h, int view_top, int view_bottom,
                        bool lead)
{
    int cw, ch;
    if (lead)
        cw = ch = pictureSize(w, LEAD_PICTURE_MAX);
    else
        uiNewsCoverSize(&cw, &ch);
    bool failed;
    char path[SHELF_PATH_MAX + 8];
    snprintf(path, sizeof(path), "%s%s", b->path, lead ? NEWS_LEAD_COVER : "");
    const Cover *cv = coversGet(&app->covers, path, lead ? 0 : b->size, cw, ch, &failed);
    if (!cv || cv->width <= 0 || cv->height <= 0)
        return false;
    float scale = (float)w / cv->width > (float)h / cv->height ? (float)w / cv->width : (float)h / cv->height;
    float dw = cv->width * scale, dh = cv->height * scale;
    clipTo(x, y, w, h, view_top, view_bottom);
    gfxDrawImageScaledFb(fb(x + (w - dw) / 2), fb(y + (h - dh) / 2), fb(dw), fb(dh), cv->width, cv->height, cv->pixels);
    gfxSetClip(0, view_top, gfxLayoutWidth(), view_bottom - view_top);
    return true;
}

// Without a picture: a card in the feed's color, its name and the article's first words.
static void drawTextCard(const ShelfBook *b, int x, int y, int w, int h, bool large)
{
    Color bg = feedColor(b);
    gfxFillRoundRect(x, y, w, h, 10, bg);
    gfxFillRect(x, y + h - 6, w, 6, gfxMix(bg, COLOR_BG, 80));
    int pad = large ? 28 : 18;
    fontDrawClipped(FONT_BOLD, large ? 20 : 16, x + pad, y + pad, w - 2 * pad - 40, b->author, COLOR_TEXT);
    char lines[5][TEXT_LINE_MAX];
    int px = large ? 20 : 15, max = (h - pad * 2 - 34) / (px + 8);
    int n = uiWrapLines(FONT_REGULAR, px, b->author_sort[0] ? b->author_sort : b->title, w - 2 * pad, lines,
                        max < 1 ? 1 : max > 5 ? 5 : max);
    for (int i = 0; i < n; i++)
        fontDraw(FONT_REGULAR, px, x + pad, y + pad + 34 + i * (px + 8), lines[i], gfxMix(COLOR_TEXT, bg, 60));
}

// Not read yet: a dot in the news' color at the picture's top right (x, y: that corner).
static void drawUnreadDot(int x, int y)
{
    gfxFillRoundRect(x - 42, y + 10, 32, 32, 16, gfxMix(COLOR_BG, COLOR_SURFACE, 128));
    gfxFillRoundRect(x - 34, y + 18, 16, 16, 8, COLOR_NEWS);
}

static void drawProgress(const ShelfBook *b, int x, int y, int w)
{
    if (b->percent <= 0 || b->percent >= 100)
        return;
    gfxFillRect(x, y - 5, w, 5, gfxMix(COLOR_BG, COLOR_SURFACE, 128));
    gfxFillRect(x, y - 5, w * b->percent / 100, 5, COLOR_ACCENT);
}

// "THE HARBOR GAZETTE · 2 h ago" as the lead's label: a bar in the news' color, then the words.
static void drawLabel(const ShelfBook *b, int x, int y, int w)
{
    char when[48], label[SHELF_TEXT_MAX + 64];
    uiNewsWhen(b, when, sizeof(when));
    snprintf(label, sizeof(label), "%s%s%s", b->author, when[0] ? "  \xC2\xB7  " : "", when);
    gfxFillRect(x, y + 9, 26, 3, COLOR_NEWS);
    fontDrawClipped(FONT_BOLD, 16, x + 38, y, w - 38, label, COLOR_NEWS);
}

static void drawLead(App *app, int index, int y, bool selected, int view_top, int view_bottom)
{
    const ShelfBook *b = &app->shelf.items[app->order[index]];
    int w = gfxLayoutWidth() - 2 * MARGIN, x = MARGIN;
    bool side = gfxLayoutWidth() >= 1000;
    int pw = side ? w * NEWS_LEAD_SHARE / 100 : w, ph = pw * 9 / 16;
    int h = side ? ph : ph + LEAD_UPRIGHT_TEXT;
    if (selected)
        gfxFillRoundRect(x - 14, y - 10, w + 28, h + 20, 16, COLOR_SURFACE_HI);
    int px = side ? x + w - pw : x;
    if (selected)
        gfxFillRoundRect(px - 5, y - 5, pw + 10, ph + 10, 12, COLOR_ACCENT);
    if (!drawPicture(app, b, px, y, pw, ph, view_top, view_bottom, true))
        drawTextCard(b, px, y, pw, ph, true);
    if (b->percent < 0)
        drawUnreadDot(px + pw, y);
    drawProgress(b, px, y + ph, pw);
    int tx = side ? x : x, ty = side ? y + 6 : y + ph + 18, tw = side ? w - pw - 36 : w;
    drawLabel(b, tx, ty, tw);
    char lines[3][TEXT_LINE_MAX];
    int title_px = side ? 34 : 28;
    int n = uiWrapLines(FONT_BOLD, title_px, b->title, tw, lines, side ? 3 : 2);
    for (int i = 0; i < n; i++)
        fontDraw(FONT_BOLD, title_px, tx, ty + 34 + i * (title_px + 10), lines[i], selected ? COLOR_ACCENT : COLOR_TEXT);
    int sy = ty + 34 + n * (title_px + 10) + 12;
    char summary[4][TEXT_LINE_MAX];
    int room = (side ? y + h : ty + LEAD_UPRIGHT_TEXT - 10) - sy;
    int max = room / 28;
    int m = b->author_sort[0] ? uiWrapLines(FONT_REGULAR, 18, b->author_sort, tw, summary, max > 4 ? 4 : max < 0 ? 0 : max) : 0;
    for (int i = 0; i < m; i++)
        fontDraw(FONT_REGULAR, 18, tx, sy + i * 28, summary[i], COLOR_DIM);
    uiHitAdd(x - 14, y - 10, w + 28, h + 20, HIT_BOOK, index);
}

static void drawCard(App *app, int index, int x, int y, bool selected, int view_top, int view_bottom)
{
    const ShelfBook *b = &app->shelf.items[app->order[index]];
    int w, ph;
    appNewsCardSize(&w, &ph);
    if (selected)
        gfxFillRoundRect(x - 6, y - 6, w + 12, ph + 12, 12, COLOR_ACCENT);
    if (!drawPicture(app, b, x, y, w, ph, view_top, view_bottom, false))
        drawTextCard(b, x, y, w, ph, false);
    if (b->percent < 0)
        drawUnreadDot(x + w, y);
    drawProgress(b, x, y + ph, w);
    char lines[2][TEXT_LINE_MAX];
    int n = uiWrapLines(FONT_BOLD, 18, b->title, w, lines, 2);
    for (int i = 0; i < n; i++)
        fontDraw(FONT_BOLD, 18, x, y + ph + 12 + i * 25, lines[i], selected ? COLOR_ACCENT : COLOR_TEXT);
    char when[48], line[SHELF_TEXT_MAX + 64];
    uiNewsWhen(b, when, sizeof(when));
    snprintf(line, sizeof(line), "%s%s%s", b->author, when[0] ? " \xC2\xB7 " : "", when);
    fontDrawClipped(FONT_REGULAR, 15, x, y + ph + 12 + 2 * 25 + 6, w, line, COLOR_DIM);
    uiHitAdd(x - 6, y - 6, w + 12, ph + NEWS_CARD_TEXT, HIT_BOOK, index);
}

void uiNewsDrawLine(App *app, const LibLine *line, int y, int view_top, int view_bottom)
{
    if (line->lead) {
        drawLead(app, line->first, y + 10, line->first == app->cursor && app->panel == PANEL_NONE, view_top, view_bottom);
        return;
    }
    int w, ph;
    appNewsCardSize(&w, &ph);
    for (int i = 0; i < line->count; i++) {
        int book = line->first + i;
        drawCard(app, book, MARGIN + i * (w + NEWS_GAP), y + 6, book == app->cursor && app->panel == PANEL_NONE,
                 view_top, view_bottom);
    }
}
