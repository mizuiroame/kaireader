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
#include "ui_hit.h"
#include "ui_notice.h"
#include "util.h"

#define PANEL_RADIUS 22
#define MENU_W 480
#define MENU_ROW_H 54
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
        T("Put EPUB, PDF, CBZ, FB2, MOBI or TXT files in the books folder of the SD card:"),
        "sdmc:/books",
        T("Books with DRM (bought in a store that locks them) can't be opened."),
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

static void drawLibrary(App *app)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxClear(COLOR_BG);
    uiDrawHeader("KaiReader");
    uiDrawStatus(&app->sys, app->prefs.clock_24h, w - MARGIN, 18, COLOR_TEXT, COLOR_DIM);

    char sub[64];
    snprintf(sub, sizeof(sub), app->order_count == 1 ? T("1 book") : T("%d books"), app->order_count);
    int tw = fontDraw(FONT_BOLD, 22, MARGIN, 74, T("Library"), COLOR_TEXT);
    fontDraw(FONT_REGULAR, 18, MARGIN + tw + 14, 77, sub, COLOR_DIM);
    if (app->order_count) {  // The sort, at the right: a tap (or Y) changes it.
        char sort[64];
        snprintf(sort, sizeof(sort), "%s", T(prefsSortName(app->prefs.sort)));
        int sw = fontTextWidth(FONT_REGULAR, 17, sort), bw = uiButtonWidth("Y");
        int x = w - MARGIN - sw;
        fontDraw(FONT_REGULAR, 17, x, 78, sort, COLOR_DIM);
        uiDrawButton(x - bw - 8, 89.0f, "Y", COLOR_SURFACE_HI, COLOR_DIM);
        uiHitAdd(x - bw - 16, 64, sw + bw + 24, 44, HIT_SORT, 0);
    }

    if (app->order_count == 0) {
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
                drawBook(app, book, left + i * LIB_CELL_W, y + 8, book == app->cursor && app->panel == PANEL_NONE);
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
    drawFooter(app->order_count ? T("[A] Read  [Y] Sort  [+] Menu") : T("[+] Menu"));
}

// --- The reader ---

static void drawPage(const App *app, int page, int offset)
{
    int x, y, w, h;
    appPageRect(app, page, &x, &y, &w, &h);
    const DocImage *img = appPageImage(app, page);
    if (img)
        gfxDrawImageScaledFb(fb(x + offset), fb(y), fb(w), fb(h), img->width, img->height, img->pixels);
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

    bool upright = gfxTurn() != GFX_TURN_NONE;
    int top = h - BARS_BOTTOM_H - (upright ? BARS_HELP_H : 0);
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
    const char *help = T("[Y] Colors  [^v] Text size  [LS] Bookmark  [RS] Turn  [-] Hide  [B] Library");
    if (upright)  // No room beside the chapter: a line of its own, under it.
        uiDrawHelp(MARGIN, top + BARS_BOTTOM_H + 12.0f, w - 2 * MARGIN, help);
    else
        uiDrawHelp(w / 2, top + 62.0f, w / 2 - MARGIN, help);
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
    case MENU_SETTINGS:
        return T("Settings");
    case MENU_RESCAN:
        return T("Look for books again");
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
    int n = appMenuRows(app, rows);
    int w = minInt(MENU_W, gfxLayoutWidth() - 40), h = 70 + n * MENU_ROW_H + 56;
    int x = (gfxLayoutWidth() - w) / 2, y = (gfxLayoutHeight() - h) / 2;
    gfxFillRoundRect(x, y, w, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, w, h, HIT_MENU_ITEM, -1);
    fontDraw(FONT_BOLD, 24, x + 28, y + 22, "KaiReader", COLOR_ACCENT);
    for (int i = 0; i < n; i++) {
        int ry = y + 70 + i * MENU_ROW_H;
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
        T("A book reader for Switch 2 homebrew. Your books from sdmc:/books: EPUB, PDF, CBZ, FB2, MOBI, TXT and XPS."),
        T("Pages are drawn with MuPDF (Artifex Software), under the GNU AGPL 3.0. KaiReader is free software under the same license; its code is on GitHub (mizuiroame/kaireader)."),
        T("Books locked with DRM can't be opened: only books you can read anywhere."),
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
        if (settingsHasValue(rows[i])) {
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
        drawLibrary(app);
    else
        drawReader(app, now_ns);
    if (app->panel == PANEL_MENU)
        drawMenu(app);
    else if (app->panel == PANEL_SETTINGS)
        drawSettings(app);
    else if (app->panel == PANEL_CONTENTS)
        drawContents(app);
    NoticeView views[NOTICE_STACK + 1];
    int n = noticeVisible(&app->notices, now_ns, views);
    uiDrawNotices(views, n);
}
