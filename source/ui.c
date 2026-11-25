// KaiReader's screens: the library (a grid of covers, in groups when sorted
// by author or genre), the reader (a page, and bars over it on demand), and
// the panels over both (the + menu, Settings in pages, the contents). Only
// drawing: what input does is in app.c. Sizes come from gfxLayoutWidth and
// gfxLayoutHeight, so the same screens work with the console upright.
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "font.h"
#include "gfx.h"
#include "lang.h"
#include "ui_brand.h"
#include "ui_buttons.h"
#include "ui_common.h"
#include "ui_marks.h"
#include "davnet.h"
#include "ui_net.h"
#include "ui_hit.h"
#include "ui_notice.h"
#include "util.h"

#define PANEL_RADIUS 22
#define MENU_W 480
#define MENU_ROW_H 54
#define COVERS_SPIN_HOLD_NS 600000000ull  // The covers' spinner stays this long after the last.
#define MENU_LINE_H 17  // Between the menu's groups: a thin line.
#define SETTINGS_W_MAX 760
#define SETTINGS_ROW_H 50
#define CONTENTS_W_MAX 780
#define BARS_TOP_H 64
#define BARS_BOTTOM_H 84
#define BARS_HELP_H 40  // Upright: the button help on a line of its own.
#define ABOUT_GAP 8        // About: Mizuiro's mark this far under the text,
#define ABOUT_BRAND_H 104  // the seal this tall.
#define TURN_BUTTON_W 56            // The reading bar's turn button,
#define TURN_BUTTON_FROM_RIGHT 290  // left of the clock and battery.

static const Color PLACEHOLDER_COLORS[] = {
    GFX_RGB(0x3D, 0x5A, 0xA8), GFX_RGB(0x2E, 0x7D, 0x6B), GFX_RGB(0x9C, 0x3D, 0x5A),
    GFX_RGB(0xA8, 0x6A, 0x2E), GFX_RGB(0x5E, 0x3D, 0x9C), GFX_RGB(0x4A, 0x6B, 0x2E),
};

static unsigned nameHash(const char *s)
{
    unsigned h = 5381;
    for (; *s; s++)
        h = h * 33 + (unsigned char)*s;
    return h;
}

static float fb(int v)
{
    return (float)v * GFX_SCALE;
}

static int minInt(int a, int b)
{
    return a < b ? a : b;
}

// An image fitted (keeping its shape) into a box of layout units, at the box's bottom, centered.
static void drawFitted(int x, int y, int w, int h, const uint32_t *pixels, int iw, int ih)
{
    float scale = (float)w / iw < (float)h / ih ? (float)w / iw : (float)h / ih;
    float dw = iw * scale, dh = ih * scale;
    gfxDrawImageScaledFb(fb(x) + (fb(w) - dw * GFX_SCALE) / 2, fb(y) + fb(h) - dh * GFX_SCALE, dw * GFX_SCALE,
                         dh * GFX_SCALE, iw, ih, pixels);
}

// Splits text into up to max_lines lines of width w (words kept whole when they can be).
static int wrapLines(FontFace face, int px, const char *text, int w, char lines[][160], int max_lines)
{
    int n = 0;
    const char *s = text;
    while (*s && n < max_lines) {
        while (*s == ' ')
            s++;
        size_t best = 0, i = 0;
        char tmp[160];
        while (s[i] && i < sizeof(tmp) - 1) {
            const char *p = s + i;
            utilNextCodePoint(&p);
            size_t next = (size_t)(p - s);
            if (next >= sizeof(tmp))
                break;
            memcpy(tmp, s, next);
            tmp[next] = 0;
            if (fontTextWidth(face, px, tmp) > w)
                break;
            i = next;
            if (!s[i] || s[i] == ' ')
                best = i;
        }
        if (!best || n == max_lines - 1)
            best = i ? i : strlen(s) < sizeof(tmp) - 1 ? strlen(s) : sizeof(tmp) - 1;
        snprintf(lines[n++], 160, "%.*s", (int)best, s);
        s += best;
    }
    return n;
}

static const char *formatOf(const char *path)
{
    const char *dot = strrchr(path, '.');
    static char ext[8];
    snprintf(ext, sizeof(ext), "%s", dot ? dot + 1 : "");
    for (char *c = ext; *c; c++)
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - 'a' + 'A');
    return ext;
}

static void drawFooter(const char *help)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxFillRect(0, h - FOOTER_H, w, FOOTER_H, COLOR_BG);
    uiDrawHelp(MARGIN, h - FOOTER_H + 28.0f, w - 2 * MARGIN, help);
}

// --- The library ---

static void drawPlaceholder(int x, int y, const ShelfBook *b, const char *name)
{
    Color bg = PLACEHOLDER_COLORS[nameHash(b->path) % (sizeof(PLACEHOLDER_COLORS) / sizeof(PLACEHOLDER_COLORS[0]))];
    gfxFillRoundRect(x, y, LIB_COVER_W, LIB_COVER_H, 8, bg);
    gfxFillRect(x + 10, y, 3, LIB_COVER_H, gfxMix(bg, COLOR_BG, 90));  // A spine.
    char lines[5][160];
    int n = wrapLines(FONT_BOLD, 19, name, LIB_COVER_W - 34, lines, 5);
    for (int i = 0; i < n; i++)
        fontDraw(FONT_BOLD, 19, x + 22, y + 20 + i * 25, lines[i], COLOR_TEXT);
    fontDraw(FONT_BOLD, 13, x + 22, y + LIB_COVER_H - 30, formatOf(b->path), gfxMix(COLOR_TEXT, bg, 110));
}

static void coverSize(int *w, int *h)
{
    *w = (int)(fb(LIB_COVER_W) * gfxDetail());
    *h = (int)(fb(LIB_COVER_H) * gfxDetail());
}

static void drawBook(App *app, int index, int x, int y, bool selected)
{
    const ShelfBook *b = &app->shelf.items[app->order[index]];
    char name[SHELF_TEXT_MAX];
    shelfDisplayName(b, name, sizeof(name));
    if (selected)
        gfxFillRoundRect(x - 6, y - 6, LIB_COVER_W + 12, LIB_COVER_H + 12, 12, COLOR_ACCENT);
    int cw, ch;
    coverSize(&cw, &ch);
    bool failed;
    const Cover *cv = coversGet(&app->covers, b->path, b->size, cw, ch, &failed);
    if (cv) {
        if (!selected)
            gfxFillRoundRect(x, y, LIB_COVER_W, LIB_COVER_H, 8, COLOR_SURFACE);
        drawFitted(x, y, LIB_COVER_W, LIB_COVER_H, cv->pixels, cv->width, cv->height);
    } else {
        drawPlaceholder(x, y, b, name);
    }
    uiHitAdd(x - 6, y - 6, LIB_COVER_W + 12, LIB_CELL_H - 12, HIT_BOOK, index);
    if (b->favorite) {  // A star in a dark disc at the cover's top right.
        gfxFillRoundRect(x + LIB_COVER_W - 38, y + 6, 32, 32, 16, gfxMix(COLOR_BG, COLOR_SURFACE, 128));
        uiDrawStar(x + LIB_COVER_W - 22.0f, y + 22.5f, 11.0f, COLOR_ACCENT);
    }

    int ty = y + LIB_COVER_H + 12;
    if (b->percent >= 0) {
        gfxFillRoundRect(x, ty, LIB_COVER_W, 4, 2, COLOR_SURFACE_HI);
        int done = LIB_COVER_W * b->percent / 100;
        if (done > 0)
            gfxFillRoundRect(x, ty, done < 4 ? 4 : done, 4, 2, COLOR_ACCENT);
    }
    fontDrawClipped(FONT_BOLD, 17, x, ty + 10, LIB_COVER_W, name, selected ? COLOR_ACCENT : COLOR_TEXT);
    // Under the title: how far in; and the author, unless the group above already says it.
    const char *author = app->prefs.sort == SHELF_SORT_AUTHOR ? "" : b->author;
    char line[SHELF_TEXT_MAX + 16];
    if (b->percent >= 100)
        snprintf(line, sizeof(line), "%s", T("Finished"));
    else if (b->percent >= 0)
        snprintf(line, sizeof(line), "%d%%%s%s", b->percent, author[0] ? " \xC2\xB7 " : "", author);
    else
        snprintf(line, sizeof(line), "%s", author[0] ? author : formatOf(b->path));
    fontDrawClipped(FONT_REGULAR, 15, x, ty + 34, LIB_COVER_W, line, COLOR_DIM);
}

// A book's row in the list view: a small cover, the title, the author, the
// kind and size of progress, and a star for a favorite.
static void drawRow(App *app, int index, int y, bool selected)
{
    const ShelfBook *b = &app->shelf.items[app->order[index]];
    int w = gfxLayoutWidth(), x = MARGIN, rw = w - 2 * MARGIN, rh = LIB_ROW_H - 8;
    char name[SHELF_TEXT_MAX];
    shelfDisplayName(b, name, sizeof(name));
    if (selected) {
        gfxFillRoundRect(x - 8, y, rw + 16, rh, 12, COLOR_SURFACE_HI);
        gfxFillRect(x - 8, y + 16, 3, rh - 32, COLOR_ACCENT);
    }
    int cx = x + 10, cy = y + (rh - LIB_ROW_COVER_H) / 2, cw, ch;
    coverSize(&cw, &ch);
    bool failed;
    const Cover *cv = coversGet(&app->covers, b->path, b->size, cw, ch, &failed);
    if (cv) {
        drawFitted(cx, cy, LIB_ROW_COVER_W, LIB_ROW_COVER_H, cv->pixels, cv->width, cv->height);
    } else {
        Color bg = PLACEHOLDER_COLORS[nameHash(b->path) % (sizeof(PLACEHOLDER_COLORS) / sizeof(PLACEHOLDER_COLORS[0]))];
        gfxFillRoundRect(cx, cy, LIB_ROW_COVER_W, LIB_ROW_COVER_H, 5, bg);
        gfxFillRect(cx + 6, cy, 2, LIB_ROW_COVER_H, gfxMix(bg, COLOR_BG, 90));
    }
    int tx = cx + LIB_ROW_COVER_W + 22, right = x + rw - 10;
    // At the right: how far in (a bar and its number), or Finished.
    int bar_w = 140, bar_x = right - bar_w;
    if (b->percent >= 0 && b->percent < 100) {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", b->percent);
        int pw = fontTextWidth(FONT_BOLD, 15, pct);
        fontDraw(FONT_BOLD, 15, right - pw, y + rh / 2 - 26, pct, COLOR_DIM);
        gfxFillRoundRect(bar_x, y + rh / 2 + 2, bar_w, 4, 2, selected ? COLOR_BG : COLOR_SURFACE_HI);
        int done = bar_w * b->percent / 100;
        if (done > 0)
            gfxFillRoundRect(bar_x, y + rh / 2 + 2, done < 4 ? 4 : done, 4, 2, COLOR_ACCENT);
    } else if (b->percent >= 100) {
        int fw = fontTextWidth(FONT_BOLD, 15, T("Finished"));
        fontDrawMiddle(FONT_BOLD, 15, right - fw, y + rh / 2.0f, T("Finished"), COLOR_DIM);
    }
    int text_w = bar_x - 24 - tx;
    if (b->favorite) {
        uiDrawStar(tx + 9.0f, y + 30.0f, 9.0f, COLOR_ACCENT);
        fontDrawClipped(FONT_BOLD, 20, tx + 24, y + 17, text_w - 24, name, selected ? COLOR_ACCENT : COLOR_TEXT);
    } else {
        fontDrawClipped(FONT_BOLD, 20, tx, y + 17, text_w, name, selected ? COLOR_ACCENT : COLOR_TEXT);
    }
    fontDrawClipped(FONT_REGULAR, 17, tx, y + 47, text_w, b->author[0] ? b->author : T("Unknown author"), COLOR_DIM);
    char kind[SHELF_TEXT_MAX + 16];
    snprintf(kind, sizeof(kind), "%s%s%s", formatOf(b->path), b->genre[0] ? " \xC2\xB7 " : "", b->genre);
    fontDrawClipped(FONT_REGULAR, 14, tx, y + 72, text_w, kind, COLOR_FAINT);
    uiHitAdd(x - 8, y, rw + 16, rh, HIT_BOOK, index);
}

static void drawGroup(const App *app, const LibLine *line, int y)
{
    const ShelfBook *b = &app->shelf.items[app->order[line->group_book]];
    char group[SHELF_TEXT_MAX];
    shelfGroupOf(b, (ShelfSort)app->prefs.sort, group, sizeof(group));
    const char *text = group[0] ? group : app->prefs.sort == SHELF_SORT_GENRE ? T("No genre") : T("No author");
    int x = appLibraryLeft(app) - 6, w = gfxLayoutWidth() - 2 * x;
    fontDrawClipped(FONT_BOLD, 21, x, y + 8, w, text, group[0] ? COLOR_TEXT : COLOR_DIM);
    gfxFillRect(x, y + 38, w, 1, COLOR_SURFACE_HI);
}

// The covers of the lines about to show are asked for before they do.
static void askAhead(App *app, int line)
{
    int cw, ch;
    coverSize(&cw, &ch);
    for (int l = line; l < app->line_count && l < line + 2; l++) {
        const LibLine *ln = &app->lines[l];
        for (int i = 0; ln->first >= 0 && i < ln->count; i++) {
            const ShelfBook *b = &app->shelf.items[app->order[ln->first + i]];
            coversGet(&app->covers, b->path, b->size, cw, ch, NULL);
        }
    }
}

static void drawEmpty(int w)
{
    const char *lines[] = {
        T("No books yet."),
        T("Put EPUB, PDF, CBZ, CBR, FB2, MOBI or TXT files in the books folder of the SD card:"),
        "sdmc:/books",
        T("KaiDrop copies them from your PC with names the console can list. Books with DRM can't be opened."),
    };
    int y = 230;
    for (int i = 0; i < 4; i++) {
        FontFace face = i == 0 || i == 2 ? FONT_BOLD : FONT_REGULAR;
        int px = i == 0 ? 28 : 19;
        char wrapped[3][160];
        int n = wrapLines(face, px, lines[i], w - 2 * MARGIN, wrapped, 3);
        for (int j = 0; j < n; j++, y += px + 12) {
            int tw = fontTextWidth(face, px, wrapped[j]);
            fontDraw(face, px, (w - tw) / 2, y, wrapped[j], i == 2 ? COLOR_ACCENT : i == 0 ? COLOR_TEXT : COLOR_DIM);
        }
        y += 18;
    }
    int bw = 220, bx = (w - bw) / 2, by = y + 20;
    gfxFillRoundRect(bx, by, bw, 48, 24, COLOR_SURFACE_HI);
    fontDrawCentered(FONT_BOLD, 19, bx + bw / 2.0f, by + 24.0f, T("Look again"), COLOR_TEXT);
    uiHitAdd(bx, by, bw, 48, HIT_RESCAN, 0);
}

static void drawLibrary(App *app, uint64_t now_ns)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxClear(COLOR_BG);
    uiDrawHeader("KaiReader");
    int pill_end = uiDrawSourcePill(app);
    int status_x = uiDrawStatus(&app->sys, app->prefs.clock_24h, w - MARGIN, 18, COLOR_TEXT, COLOR_DIM);
    if (now_ns - app->covers_busy_ns < COVERS_SPIN_HOLD_NS)  // Covers being read: a spinner left of the clock.
        uiDrawCoversPill(pill_end, status_x - 18, now_ns, app->lib_tab);

    char sub[160];
    if (app->lib_tab == LIB_FREE)  // Free books: the list shown, not a count (its end isn't known).
        snprintf(sub, sizeof(sub), "%s", appFreeListName(app));
    else
        snprintf(sub, sizeof(sub), app->order_count == 1 ? T("1 book") : T("%d books"), app->order_count);
    bool remote = app->lib_tab != LIB_CARD;
    int tw = fontDraw(FONT_BOLD, 22, MARGIN, 74, remote ? uiSourceName(app->lib_tab) : T("Library"), COLOR_TEXT);
    int sw0 = fontDraw(FONT_REGULAR, 18, MARGIN + tw + 14, 77, sub, COLOR_DIM);
    if (remote)  // The folder or catalog being looked through, after the count.
        uiDrawNetScan(app->lib_tab, MARGIN + tw + 14 + sw0 + 20, 89.0f, now_ns);
    if (app->order_count || app->prefs.sort == SHELF_SORT_FAVORITES || app->lib_tab == LIB_FREE) {
        // The sort, at the right: a tap (or Y) changes it. Free books: the search there.
        char sort[64];
        snprintf(sort, sizeof(sort), "%s", app->lib_tab == LIB_FREE ? T("Search") : T(prefsSortName(app->prefs.sort)));
        int sw = fontTextWidth(FONT_REGULAR, 17, sort), bw = uiButtonWidth("Y");
        int x = w - MARGIN - sw;
        fontDraw(FONT_REGULAR, 17, x, 78, sort, COLOR_DIM);
        uiDrawButton(x - bw - 8, 89.0f, "Y", COLOR_SURFACE_HI, COLOR_DIM);
        uiHitAdd(x - bw - 16, 64, sw + bw + 24, 44, HIT_SORT, 0);
    }

    if (app->order_count == 0 && remote) {
        uiDrawNetEmpty(app, now_ns);
    } else if (app->order_count == 0 && app->prefs.sort == SHELF_SORT_FAVORITES) {
        fontDrawCentered(FONT_BOLD, 26, w / 2.0f, 250.0f, T("No favorites yet."), COLOR_TEXT);
        fontDrawCentered(FONT_REGULAR, 19, w / 2.0f, 296.0f,
                         T("Hold your finger on a cover, or choose \"Add to favorites\" in the + menu."), COLOR_DIM);
        fontDrawCentered(FONT_REGULAR, 19, w / 2.0f, 330.0f, T("Y shows the other books."), COLOR_DIM);
    } else if (app->order_count == 0) {
        drawEmpty(w);
    } else {
        int view = appLibraryViewH();
        gfxSetClip(0, LIB_TOP - 8, w, view + 8);
        uiHitClip(LIB_TOP - 8, LIB_TOP + view);
        uiHitAdd(0, LIB_TOP - 8, w, view + 8, HIT_LIBRARY, 0);
        int left = appLibraryLeft(app), first_shown = -1;
        for (int l = 0; l < app->line_count; l++) {
            const LibLine *ln = &app->lines[l];
            int y = LIB_TOP + ln->y - app->scroll;
            if (y + ln->h < LIB_TOP - 8 || y > LIB_TOP + view)
                continue;
            if (first_shown < 0)
                first_shown = l;
            if (ln->first < 0) {
                drawGroup(app, ln, y);
                continue;
            }
            for (int i = 0; i < ln->count; i++) {
                int book = ln->first + i;
                bool selected = book == app->cursor && app->panel == PANEL_NONE;
                if (app->prefs.list_view)
                    drawRow(app, book, y + 4, selected);
                else
                    drawBook(app, book, left + i * LIB_CELL_W, y + 8, selected);
            }
        }
        // The next lines' covers are asked for before they show.
        int last_shown = first_shown;
        while (last_shown + 1 < app->line_count &&
               LIB_TOP + app->lines[last_shown + 1].y - app->scroll <= LIB_TOP + view)
            last_shown++;
        askAhead(app, last_shown + 1);
        uiHitClip(0, h);
        gfxResetClip();
    }
    // The help: X names the source it goes to.
    bool free_books = app->lib_tab == LIB_FREE;
    bool sortable = app->order_count || app->prefs.sort == SHELF_SORT_FAVORITES;
    char help[200];
    snprintf(help, sizeof(help), "%s%s%s[X] %s  ", app->order_count ? T("[A] Read  ") : "",
             free_books ? T("[Y] Search  [L][R] Lists  ") : sortable ? T("[Y] Sort  ") : "", "",
             uiSourceName(appNextSource(app)));
    snprintf(help + strlen(help), sizeof(help) - strlen(help), "%s", T("[+] Menu"));
    drawFooter(help);
    uiDrawNetCopy(app);
}

// --- The reader ---

static void drawPage(const App *app, int page, int offset)
{
    int x, y, w, h;
    appPageRect(app, page, &x, &y, &w, &h);
    const DocImage *img = appPageImage(app, page);
    if (img)
        gfxDrawImageScaledFb(fb(x + offset), fb(y), fb(w), fb(h), img->width, img->height, img->pixels);
    const PagePart *part = appPagePart(app, page);  // Zoomed past what a whole page can be: the part shown, sharp.
    if (part) {
        float sx = fb(w) / part->full_w, sy = fb(h) / part->full_h;
        gfxDrawImageScaledFb(fb(x + offset) + part->x * sx, fb(y) + part->y * sy, part->image.width * sx,
                             part->image.height * sy, part->image.width, part->image.height, part->image.pixels);
    }
}

// The page shown at first, and the next one beside it when two show.
static void drawShown(const App *app, int first, int offset)
{
    drawPage(app, first, offset);
    if (appTwoPages(app) && first + 1 < app->pages)
        drawPage(app, first + 1, offset);
}

// The reading bar's turn button: a phone-shaped outline with an arrow going
// round it. A tap stands the screen up or lays it down (as the right stick's press).
static void drawTurnButton(int x, int y)
{
    gfxFillRoundRect(x, y, TURN_BUTTON_W, 44, 22, COLOR_SURFACE_HI);
    float cx = x + TURN_BUTTON_W / 2.0f, cy = y + 22.0f;
    gfxFillRoundRect((int)cx - 6, (int)cy - 10, 12, 20, 3, COLOR_TEXT);
    gfxFillRoundRect((int)cx - 4, (int)cy - 8, 8, 16, 2, COLOR_SURFACE_HI);
    gfxArc(cx, cy, 15.0f, 2.2f, 3.6f, 5.6f, COLOR_ACCENT);
    gfxTriangle(cx + 9.0f, cy - 15.0f, cx + 15.5f, cy - 9.5f, cx + 7.0f, cy - 7.0f, COLOR_ACCENT);
    uiHitAdd(x - 4, 0, TURN_BUTTON_W + 8, BARS_TOP_H, HIT_TURN, 0);
}

static void drawBars(const App *app)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    const ShelfBook *b = &app->shelf.items[app->book];
    gfxBlendRect(0, 0, w, BARS_TOP_H, COLOR_BG, 240);
    // Back to the library: an arrow.
    float cx = MARGIN + 10.0f, cy = 32.0f;
    gfxLine(cx + 6, cy - 10, cx - 4, cy, 3.0f, COLOR_TEXT);
    gfxLine(cx - 4, cy, cx + 6, cy + 10, 3.0f, COLOR_TEXT);
    uiHitAdd(0, 0, MARGIN + 40, BARS_TOP_H, HIT_BACK, 0);
    char name[SHELF_TEXT_MAX];
    shelfDisplayName(b, name, sizeof(name));
    int title_w = w - 2 * MARGIN - TURN_BUTTON_FROM_RIGHT - MARK_BUTTON_W - 80;  // Clear of the buttons.
    fontDrawClipped(FONT_BOLD, 22, MARGIN + 40, 10, title_w, name, COLOR_TEXT);
    if (b->author[0])
        fontDrawClipped(FONT_REGULAR, 15, MARGIN + 40, 38, title_w, b->author, COLOR_DIM);
    uiDrawStatus(&app->sys, app->prefs.clock_24h, w - MARGIN, 18, COLOR_TEXT, COLOR_DIM);
    drawTurnButton(w - MARGIN - TURN_BUTTON_FROM_RIGHT, 8);
    uiDrawMarkButton(app, w - MARGIN - TURN_BUTTON_FROM_RIGHT - MARK_BUTTON_W - 12, 8);

    int top = h - BARS_BOTTOM_H - BARS_HELP_H;  // The button help on a line of its own: it's long.
    gfxBlendRect(0, top, w, h - top, COLOR_BG, 240);
    int bw = 120;
    gfxFillRoundRect(MARGIN, top + 12, bw, 36, 18, COLOR_SURFACE_HI);
    fontDrawCentered(FONT_BOLD, 16, MARGIN + bw / 2.0f, top + 30.0f, T("Contents"), COLOR_TEXT);
    uiHitAdd(MARGIN, top + 6, bw, 48, HIT_CONTENTS, 0);

    int left = UI_SLIDER_LEFT, width = w - 2 * left, sy = top + 30;
    gfxFillRoundRect(left, sy - 3, width, 6, 3, COLOR_SURFACE_HI);
    float t = app->pages > 1 ? (float)app->page / (app->pages - 1) : 0;
    int knob = left + (int)(t * width);
    gfxFillRoundRect(left, sy - 3, knob - left, 6, 3, COLOR_ACCENT);
    gfxFillCircle(knob, sy, 10, COLOR_ACCENT);
    uiHitAdd(left - 20, top + 4, width + 40, 52, HIT_SLIDER, 0);
    char where[64];
    if (appTwoPages(app) && app->page + 1 < app->pages)
        snprintf(where, sizeof(where), "%d\xE2\x80\x93%d / %d", app->page + 1, app->page + 2, app->pages);
    else
        snprintf(where, sizeof(where), T("%d of %d"), app->page + 1, app->pages);
    int ww = fontTextWidth(FONT_BOLD, 17, where);
    fontDraw(FONT_BOLD, 17, w - MARGIN - ww, top + 18, where, COLOR_TEXT);

    int ch = appChapterOf(app, app->page);
    if (ch >= 0)
        fontDrawClipped(FONT_REGULAR, 16, left, top + 50, width / 2, app->chapters[ch].title, COLOR_DIM);
    const char *help = app->reflowable
                           ? T("[X] Contents  [Y] Colors  [^v] Text size  [LS] Bookmark  [RS] Turn  [-] Hide  [B] Library")
                           : T("[X] Contents  [^v] Zoom  [LS] Bookmark  [RS] Turn  [-] Hide  [B] Library");
    uiDrawHelp(MARGIN, top + BARS_BOTTOM_H + 12.0f, w - 2 * MARGIN, help);
}

static void drawReader(App *app, uint64_t now_ns)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    DocColors colors = prefsColors(app->prefs.theme);
    Color paper = uiColorOf(colors.paper), ink = uiColorOf(colors.ink);
    bool flowing = !app->ready || app->reflowable;
    gfxClear(flowing ? paper : GFX_RGB(0x0E, 0x0E, 0x10));
    if (!flowing)
        ink = COLOR_DIM;
    uiHitAdd(0, 0, w, h, HIT_PAGE, 0);
    if (!app->ready) {
        uiDrawSpinner(w / 2.0f, h / 2.0f - 20, 22, now_ns, ink);
        fontDrawCentered(FONT_REGULAR, 18, w / 2.0f, h / 2.0f + 30, T("Opening the book\xE2\x80\xA6"),
                         gfxMix(ink, paper, 80));
        return;
    }
    if (app->turn_from >= 0) {
        float t = (float)(now_ns - app->turn_start_ns) / READ_TURN_NS;
        t = t > 1 ? 1 : t;
        float eased = 1 - (1 - t) * (1 - t) * (1 - t);
        int shift = (int)(eased * w);
        drawShown(app, app->turn_from, -app->turn_dir * shift);
        drawShown(app, app->page, app->turn_dir * (w - shift));
    } else if (appPageImage(app, app->page)) {
        drawShown(app, app->page, 0);
    } else {
        uiDrawSpinner(w / 2.0f, h / 2.0f, 18, now_ns, gfxMix(ink, paper, 120));
    }
    if (flowing && !app->bars) {
        // Under the page, faint: the chapter and where in the book.
        Color faint = gfxMix(ink, paper, 150);
        // With two pages, each its number under it, and the chapter under the left one.
        bool two = appTwoPages(app);
        int ch = appChapterOf(app, app->page);
        for (int side = 0; side < (two ? 2 : 1) && app->page + side < app->pages; side++) {
            int cx, cy, cw, chh;
            appColumn(app, side, &cx, &cy, &cw, &chh);
            char where[32];
            snprintf(where, sizeof(where), two ? "%d" : "%d / %d", app->page + side + 1, app->pages);
            int y = cy + chh + 8, ww = fontTextWidth(FONT_REGULAR, 15, where);
            fontDraw(FONT_REGULAR, 15, cx + cw - ww, y, where, faint);
            if (side == 0 && ch >= 0)
                fontDrawClipped(FONT_REGULAR, 15, cx, y, cw - ww - 40, app->chapters[ch].title, faint);
        }
    }
    uiDrawPageRibbons(app);
    if (app->bars)
        drawBars(app);
}

// --- Panels ---

static void dim(void)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxBlendRect(0, 0, w, h, GFX_RGB(0, 0, 0), 150);
    uiHitAdd(0, 0, w, h, HIT_OVERLAY_OUT, 0);
}

static const char *menuLabel(const App *app, MenuItem item)
{
    switch (item) {
    case MENU_CONTENTS:
        return T("Contents and bookmarks");
    case MENU_BOOKMARK:
        return appShownMarked(app) ? T("Remove this page's bookmark") : T("Bookmark this page");
    case MENU_LIBRARY:
        return T("Back to the library");
    case MENU_FAVORITE: {
        const ShelfBook *b = appSelectedBook(app);
        return b && b->favorite ? T("Remove from favorites") : T("Add to favorites");
    }
    case MENU_VIEW:
        return app->prefs.list_view ? T("Show as a grid") : T("Show as a list");
    case MENU_SOURCE:
        return appNextSource(app) == LIB_NETWORK  ? T("Go to the network folder")
               : appNextSource(app) == LIB_SERVER ? T("Go to the book server")
               : appNextSource(app) == LIB_FREE   ? T("Go to free books")
                                                  : T("Go to the SD card library");
    case MENU_FREE_SEARCH:
        return T("Search Project Gutenberg");
    case MENU_FREE_LIST:
        return app->free_list == 0 ? T("Show Standard Ebooks' newest") : T("Show Project Gutenberg's most popular");
    case MENU_NET_SETUP:
        return T("Add a network folder or book server");
    case MENU_SETTINGS:
        return T("Settings");
    case MENU_RESCAN:
        return app->lib_tab == LIB_NETWORK  ? T("Look through the network folder again")
               : app->lib_tab == LIB_SERVER ? T("Look through the server's catalog again")
               : app->lib_tab == LIB_FREE   ? T("Load the list again")
                                            : T("Look for books again");
    case MENU_EXIT:
        return T("Exit KaiReader");
    default:
        return "";
    }
}

static void drawMenu(const App *app)
{
    dim();
    MenuItem rows[MENU_COUNT];
    int n = appMenuRows(app, rows), lines = 0;
    for (int i = 1; i < n; i++)
        lines += appMenuGroup(rows[i]) != appMenuGroup(rows[i - 1]);
    int w = minInt(MENU_W, gfxLayoutWidth() - 40), h = 70 + n * MENU_ROW_H + lines * MENU_LINE_H + 56;
    int x = (gfxLayoutWidth() - w) / 2, y = (gfxLayoutHeight() - h) / 2;
    gfxFillRoundRect(x, y, w, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, w, h, HIT_MENU_ITEM, -1);
    int name_w = fontDraw(FONT_BOLD, 24, x + 28, y + 22, "KaiReader", COLOR_ACCENT);
    fontDraw(FONT_REGULAR, 17, x + 28 + name_w + 10, y + 28, KAIREADER_VERSION, COLOR_DIM);
    int ry = y + 70 - MENU_ROW_H;
    for (int i = 0; i < n; i++) {
        ry += MENU_ROW_H;
        if (i > 0 && appMenuGroup(rows[i]) != appMenuGroup(rows[i - 1])) {  // A new group: a line before it.
            gfxFillRect(x + 28, ry + MENU_LINE_H / 2 - 4, w - 56, 1, COLOR_SURFACE_HI);
            ry += MENU_LINE_H;
        }
        bool on = i == app->menu_cursor;
        if (on)
            gfxFillRoundRect(x + 12, ry, w - 24, MENU_ROW_H - 4, 12, COLOR_SURFACE_HI);
        fontDrawMiddle(FONT_BOLD, 19, x + 28, ry + (MENU_ROW_H - 4) / 2.0f, menuLabel(app, rows[i]),
                       on ? COLOR_ACCENT : COLOR_TEXT);
        uiHitAdd(x + 12, ry, w - 24, MENU_ROW_H - 4, HIT_MENU_ITEM, i);
    }
    uiDrawHelp(x + 28, y + h - 30.0f, w - 56, T("[A] Choose  [B] Close"));
}

// About's paragraphs from y down; returns where they end. Measures only, without draw.
static int drawAboutText(int x, int y, int w, bool draw)
{
    const char *paragraphs[] = {
        "KaiReader " KAIREADER_VERSION,
        T("A book reader for Switch 2 homebrew. Your books from sdmc:/books, a network folder or a book server (OPDS): EPUB, PDF, CBZ, CBR, CB7, FB2, MOBI, TXT and XPS."),
        T("Pages are drawn with MuPDF (Artifex Software), under the GNU AGPL 3.0. KaiReader is free software under the same license; its code is on GitHub (mizuiroame/kaireader)."),
        T("Books locked with DRM can't be opened: only books you can read anywhere. Comics in archives use libarchive; network folders use libsmb2 (LGPL 2.1), curl, libssh2 and Mbed TLS. Their licenses are in docs/legal.md on GitHub."),
    };
    for (int p = 0; p < 4; p++) {
        char lines[6][160];
        int n = wrapLines(p ? FONT_REGULAR : FONT_BOLD, p ? 18 : 22, paragraphs[p], w, lines, 6);
        for (int i = 0; i < n; i++, y += p ? 27 : 32)
            if (draw)
                fontDraw(p ? FONT_REGULAR : FONT_BOLD, p ? 18 : 22, x, y, lines[i],
                         p == 0 ? COLOR_ACCENT : p == 1 ? COLOR_TEXT : COLOR_DIM);
        y += 14;
    }
    return y;
}

static void drawSettings(const App *app)
{
    dim();
    SettingRow rows[SETTINGS_ROWS_MAX];
    int n = settingsRows(app->settings_page, rows);
    bool about = app->settings_page == SETTINGS_PAGE_ABOUT;
    int w = minInt(SETTINGS_W_MAX, gfxLayoutWidth() - 40);
    int about_text = about ? drawAboutText(0, 0, w - 64, false) : 0;  // Its height, to fit the page to it.
    int h = about ? 90 + about_text + ABOUT_GAP + ABOUT_BRAND_H + 64 : 84 + n * SETTINGS_ROW_H + 110;
    int x = (gfxLayoutWidth() - w) / 2, y = (gfxLayoutHeight() - h) / 2;
    gfxFillRoundRect(x, y, w, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, w, h, HIT_SETTING, -1);
    int tx = x + 28;
    if (app->settings_page != SETTINGS_PAGE_MAIN) {  // Back to the main page.
        int bw = fontTextWidth(FONT_BOLD, 16, T("Back")) + 44;
        gfxFillRoundRect(x + 28, y + 22, bw, 36, 18, COLOR_SURFACE_HI);
        gfxFillTriangleLeft(x + 42, y + 33, 10, 14, COLOR_TEXT);
        fontDrawMiddle(FONT_BOLD, 16, x + 60, y + 40.0f, T("Back"), COLOR_TEXT);
        uiHitAdd(x + 20, y + 14, bw + 16, 52, HIT_SETTINGS_BACK, 0);
        tx = x + 28 + bw + 18;
    }
    fontDrawMiddle(FONT_BOLD, 22, tx, y + 40.0f, T(settingsPageTitle(app->settings_page)), COLOR_TEXT);
    if (about) {
        int end = drawAboutText(x + 32, y + 90, w - 64, true);
        uiDrawBrand(x + 32, end + ABOUT_GAP, ABOUT_BRAND_H);  // Mizuiro's seal and name, right under it.
        uiDrawHelp(x + 28, y + h - 30.0f, w - 56, T("[B] Back"));
        return;
    }
    for (int i = 0; i < n; i++) {
        int ry = y + 84 + i * SETTINGS_ROW_H;
        bool on = i == app->settings_cursor;
        if (on) {
            gfxFillRoundRect(x + 14, ry, w - 28, SETTINGS_ROW_H - 4, 10, COLOR_SURFACE_HI);
            gfxFillRect(x + 14, ry + 12, 3, SETTINGS_ROW_H - 28, COLOR_ACCENT);
        }
        float mid = ry + (SETTINGS_ROW_H - 4) / 2.0f;
        fontDrawMiddle(on ? FONT_BOLD : FONT_REGULAR, 19, x + 32, mid, T(settingsLabel(rows[i])), COLOR_TEXT);
        int right = x + w - 34;
        if (settingsPickedValue(rows[i])) {  // A value picked with A (the keyboard): no arrows.
            char value[160];
            bool name;
            settingsValue(&app->prefs, rows[i], value, sizeof(value), &name);
            const char *shown = name ? T(value) : value;
            int vw = fontTextWidth(FONT_BOLD, 18, shown), max_vw = w / 2 - 40;
            vw = vw > max_vw ? max_vw : vw;
            gfxSetClip(right - vw, ry, vw, SETTINGS_ROW_H);
            fontDrawMiddle(FONT_BOLD, 18, right - vw, mid, shown, COLOR_ACCENT);
            gfxResetClip();
            uiHitAdd(x + 14, ry, w - 28, SETTINGS_ROW_H - 4, HIT_SETTING, i);
        } else if (settingsHasValue(rows[i])) {
            char value[64];
            bool name;
            settingsValue(&app->prefs, rows[i], value, sizeof(value), &name);
            const char *shown = name ? T(value) : value;
            int vw = fontTextWidth(FONT_BOLD, 18, shown);
            int max_vw = w / 2 - 60;
            vw = vw > max_vw ? max_vw : vw;
            gfxFillTriangleRight(right - 10, (int)mid - 8, 10, 16, on ? COLOR_TEXT : COLOR_FAINT);
            gfxSetClip(right - 22 - vw, ry, vw, SETTINGS_ROW_H);  // A long value is cut at its room.
            fontDrawMiddle(FONT_BOLD, 18, right - 22 - vw, mid, shown, COLOR_ACCENT);
            gfxResetClip();
            gfxFillTriangleLeft(right - 42 - vw, (int)mid - 8, 10, 16, on ? COLOR_TEXT : COLOR_FAINT);
            int half = (w - 28) / 2;
            uiHitAdd(x + 14, ry, half, SETTINGS_ROW_H - 4, HIT_SETTING_STEP, i * 2);
            uiHitAdd(x + 14 + half, ry, w - 28 - half, SETTINGS_ROW_H - 4, HIT_SETTING_STEP, i * 2 + 1);
        } else {
            if (settingsOpens(rows[i]) != SETTINGS_PAGE_MAIN)  // A page: an arrow into it.
                gfxFillTriangleRight(right - 10, (int)mid - 8, 10, 16, on ? COLOR_TEXT : COLOR_FAINT);
            uiHitAdd(x + 14, ry, w - 28, SETTINGS_ROW_H - 4, HIT_SETTING, i);
        }
    }
    // The help of the row selected.
    if (app->settings_cursor < n) {
        char lines[3][160];
        int lines_n = wrapLines(FONT_REGULAR, 16, T(settingsHelp(rows[app->settings_cursor])), w - 64, lines, 3);
        int hy = y + 84 + n * SETTINGS_ROW_H + 10;
        for (int i = 0; i < lines_n; i++)
            fontDraw(FONT_REGULAR, 16, x + 32, hy + i * 22, lines[i], COLOR_DIM);
    }
    uiDrawHelp(x + 28, y + h - 30.0f, w - 56,
               app->settings_page == SETTINGS_PAGE_MAIN ? T("[A] Open  [B] Close") : T("[<>] Change  [A] Choose  [B] Back"));
}

// The first run's welcome: where are the books? A row each, a line under it.
#define WELCOME_W 700
#define WELCOME_ROW_H 74

static void drawWelcome(const App *app)
{
    static const char *const TITLES[WELCOME_ROW_COUNT] = {
        N_("On the SD card"), N_("A network folder"), N_("A book server"), N_("Free books"), N_("Later"),
    };
    static const char *const NOTES[WELCOME_ROW_COUNT] = {
        N_("Copy them to sdmc:/books; KaiDrop does it from your PC."),
        N_("A shared folder, a NAS or Nextcloud, FTP or SFTP."),
        N_("Calibre-Web, Kavita or Komga, by its address."),
        N_("Public-domain classics: Project Gutenberg, Standard Ebooks."),
        N_("Start with an empty library; it's all in + > Settings."),
    };
    dim();
    int w = minInt(WELCOME_W, gfxLayoutWidth() - 40), h = 116 + WELCOME_ROW_COUNT * WELCOME_ROW_H + 64;
    int x = (gfxLayoutWidth() - w) / 2, y = (gfxLayoutHeight() - h) / 2;
    gfxFillRoundRect(x, y, w, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, w, h, HIT_SETTING, -1);  // The panel itself: nothing.
    fontDrawClipped(FONT_BOLD, 26, x + 32, y + 26, w - 64, T("Welcome! Where are your books?"), COLOR_TEXT);
    fontDrawClipped(FONT_REGULAR, 17, x + 32, y + 68, w - 64, T("You can change it any time in + > Settings."), COLOR_DIM);
    int list_y = y + 110;
    for (int i = 0; i < WELCOME_ROW_COUNT; i++) {
        int ry = list_y + i * WELCOME_ROW_H;
        bool on = i == app->welcome_cursor;
        if (on) {
            gfxFillRoundRect(x + 14, ry, w - 28, WELCOME_ROW_H - 6, 12, COLOR_SURFACE_HI);
            gfxFillRect(x + 14, ry + 14, 3, WELCOME_ROW_H - 34, COLOR_ACCENT);
        }
        fontDrawClipped(on ? FONT_BOLD : FONT_REGULAR, 21, x + 36, ry + 10, w - 72, T(TITLES[i]),
                        i == WELCOME_LATER ? COLOR_DIM : on ? COLOR_ACCENT : COLOR_TEXT);
        fontDrawClipped(FONT_REGULAR, 16, x + 36, ry + 40, w - 72, T(NOTES[i]), COLOR_DIM);
        uiHitAdd(x + 14, ry, w - 28, WELCOME_ROW_H - 6, HIT_WELCOME_ROW, i);
    }
    uiDrawHelp(x + 32, y + h - 30.0f, w - 64, T("[A] Choose  [B] Later"));
}

static void drawContents(const App *app)
{
    dim();
    int max_rows = appContentsRows();
    int count = app->contents_tab ? app->book_mark_count : app->chapter_count;
    int rows = count < max_rows ? count : max_rows;
    if (rows < 2)
        rows = 2;  // Room for the empty tab's words.
    int w = minInt(CONTENTS_W_MAX, gfxLayoutWidth() - 40);
    int h = 70 + rows * CONTENTS_ROW_H + 50, x = (gfxLayoutWidth() - w) / 2, y = (gfxLayoutHeight() - h) / 2;
    gfxFillRoundRect(x, y, w, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, w, h, HIT_CHAPTER, -1);
    uiDrawContentsTabs(app, x + 28, y + 20);
    if (app->contents_tab) {
        uiDrawMarksList(app, x + 12, y + 70, w - 24, rows);
        if (count > rows) {
            int track = rows * CONTENTS_ROW_H, top = y + 70;
            int bar = track * rows / count;
            int at = top + (track - bar) * app->contents_first / (count - rows);
            gfxFillRoundRect(x + w - 8, at, 4, bar, 2, COLOR_FAINT);
        }
        uiDrawHelp(x + 28, y + h - 28.0f, w - 56, T("[A] Go  [Y] Remove  [L][R] Tabs  [B] Close"));
        return;
    }
    int current = appChapterOf(app, app->page);
    for (int r = 0; r < rows; r++) {
        int i = app->contents_first + r;
        if (i >= app->chapter_count)
            break;
        const DocChapter *c = &app->chapters[i];
        int ry = y + 70 + r * CONTENTS_ROW_H;
        bool on = i == app->contents_cursor;
        if (on)
            gfxFillRoundRect(x + 12, ry, w - 24, CONTENTS_ROW_H - 4, 10, COLOR_SURFACE_HI);
        int indent = (c->level > 4 ? 4 : c->level) * 26;
        char page[16] = "";
        if (c->page >= 0)
            snprintf(page, sizeof(page), "%d", c->page + 1);
        int pw = fontTextWidth(FONT_REGULAR, 16, page);
        float mid = ry + (CONTENTS_ROW_H - 4) / 2.0f;
        Color color = i == current ? COLOR_ACCENT : c->page < 0 ? COLOR_FAINT : COLOR_TEXT;
        gfxSetClip(x + 28 + indent, ry, w - 100 - indent - pw, CONTENTS_ROW_H);
        fontDrawMiddle(c->level ? FONT_REGULAR : FONT_BOLD, 18, x + 28 + indent, mid,
                       c->title[0] ? c->title : "\xE2\x80\x94", color);
        gfxResetClip();
        fontDrawMiddle(FONT_REGULAR, 16, x + w - 32 - pw, mid, page, COLOR_DIM);
        uiHitAdd(x + 12, ry, w - 24, CONTENTS_ROW_H - 4, HIT_CHAPTER, i);
    }
    if (app->chapter_count > rows) {  // Where the rows shown are in the whole list.
        int track = rows * CONTENTS_ROW_H, top = y + 70;
        int bar = track * rows / app->chapter_count;
        int at = top + (track - bar) * app->contents_first / (app->chapter_count - rows);
        gfxFillRoundRect(x + w - 8, at, 4, bar, 2, COLOR_FAINT);
    }
    if (app->chapter_count == 0)
        fontDrawClipped(FONT_REGULAR, 17, x + 28, y + 84, w - 56, T("This book has no table of contents."), COLOR_DIM);
    uiDrawHelp(x + 28, y + h - 28.0f, w - 56, T("[A] Go  [<>] Page  [L][R] Tabs  [B] Close"));
}

void uiDraw(App *app, uint64_t now_ns)
{
    uiHitReset();
    if (app->view == VIEW_LIBRARY)
        drawLibrary(app, now_ns);
    else
        drawReader(app, now_ns);
    if (app->panel == PANEL_MENU)
        drawMenu(app);
    else if (app->panel == PANEL_SETTINGS)
        drawSettings(app);
    else if (app->panel == PANEL_CONTENTS)
        drawContents(app);
    else if (app->panel == PANEL_WELCOME)
        drawWelcome(app);
    NoticeView views[NOTICE_STACK + 1];
    int n = noticeVisible(&app->notices, now_ns, views);
    uiDrawNotices(views, n);
}
