// KaiReader's screens: the library (a grid of covers), the reader (a page,
// and bars over it on demand), and the panels over both (menu, contents,
// about). Only drawing: what input does is in app.c.
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "font.h"
#include "gfx.h"
#include "lang.h"
#include "ui_buttons.h"
#include "ui_common.h"
#include "ui_hit.h"
#include "ui_notice.h"
#include "util.h"

#define PANEL_RADIUS 22
#define MENU_W 560
#define MENU_ROW_H 54
#define CONTENTS_W 780
#define CONTENTS_ROW_H 48
#define SLIDER_LEFT (MARGIN + 140)
#define BARS_TOP_H 64
#define BARS_BOTTOM_Y 636

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

// An image fitted (keeping its shape) into a box of layout units, at the box's bottom, centered.
static void drawFitted(int x, int y, int w, int h, const uint32_t *pixels, int iw, int ih)
{
    float scale = (float)w / iw < (float)h / ih ? (float)w / iw : (float)h / ih;
    float dw = iw * scale, dh = ih * scale;
    gfxDrawImageScaledFb(fb(x) + (fb(w) - dw * GFX_SCALE) / 2, fb(y) + fb(h) - dh * GFX_SCALE, dw * GFX_SCALE,
                         dh * GFX_SCALE, iw, ih, pixels);
}

// Splits text into up to max_lines lines of width w (words kept whole when they can be).
static int wrapLines(FontFace face, int px, const char *text, int w, char lines[][96], int max_lines)
{
    int n = 0;
    const char *s = text;
    while (*s && n < max_lines) {
        while (*s == ' ')
            s++;
        size_t best = 0, i = 0;
        char tmp[96];
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
        snprintf(lines[n++], 96, "%.*s", (int)best, s);
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

// --- The library ---

static void drawPlaceholder(int x, int y, const ShelfBook *b, const char *name)
{
    Color bg = PLACEHOLDER_COLORS[nameHash(b->path) % (sizeof(PLACEHOLDER_COLORS) / sizeof(PLACEHOLDER_COLORS[0]))];
    gfxFillRoundRect(x, y, LIB_COVER_W, LIB_COVER_H, 8, bg);
    gfxFillRect(x + 10, y, 3, LIB_COVER_H, gfxMix(bg, COLOR_BG, 90));  // A spine.
    char lines[5][96];
    int n = wrapLines(FONT_BOLD, 19, name, LIB_COVER_W - 34, lines, 5);
    for (int i = 0; i < n; i++)
        fontDraw(FONT_BOLD, 19, x + 22, y + 20 + i * 25, lines[i], COLOR_TEXT);
    fontDraw(FONT_BOLD, 13, x + 22, y + LIB_COVER_H - 30, formatOf(b->path), gfxMix(COLOR_TEXT, bg, 110));
}

static void drawBook(App *app, int index, int x, int y, bool selected)
{
    const ShelfBook *b = &app->shelf.items[app->order[index]];
    char name[SHELF_TEXT_MAX];
    shelfDisplayName(b, name, sizeof(name));
    if (selected)
        gfxFillRoundRect(x - 6, y - 6, LIB_COVER_W + 12, LIB_COVER_H + 12, 12, COLOR_ACCENT);
    float detail = gfxDetail();
    bool failed;
    const Cover *cv = coversGet(&app->covers, b->path, b->size, (int)(fb(LIB_COVER_W) * detail),
                                (int)(fb(LIB_COVER_H) * detail), &failed);
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
    char line[SHELF_TEXT_MAX + 16];
    if (b->percent >= 100)
        snprintf(line, sizeof(line), "%s", T("Finished"));
    else if (b->percent >= 0)
        snprintf(line, sizeof(line), "%d%%%s%s", b->percent, b->author[0] ? " \xC2\xB7 " : "", b->author);
    else
        snprintf(line, sizeof(line), "%s", b->author[0] ? b->author : formatOf(b->path));
    fontDrawClipped(FONT_REGULAR, 15, x, ty + 34, LIB_COVER_W, line, COLOR_DIM);
}

static void drawLibrary(App *app)
{
    gfxClear(COLOR_BG);
    uiDrawHeader("KaiReader");
    uiDrawStatus(&app->sys, app->prefs.clock_24h, GFX_WIDTH - MARGIN, 18, COLOR_TEXT, COLOR_DIM);

    char sub[64];
    snprintf(sub, sizeof(sub), app->order_count == 1 ? T("1 book") : T("%d books"), app->order_count);
    int w = fontDraw(FONT_BOLD, 22, MARGIN, 74, T("Library"), COLOR_TEXT);
    fontDraw(FONT_REGULAR, 18, MARGIN + w + 14, 77, sub, COLOR_DIM);

    if (app->order_count == 0) {
        const char *lines[] = {
            T("No books yet."),
            T("Put EPUB, PDF, CBZ, FB2, MOBI or TXT files in the books folder of the SD card:"),
            "sdmc:/books",
            T("Books with DRM (bought in a store that locks them) can't be opened."),
        };
        for (int i = 0; i < 4; i++) {
            FontFace face = i == 0 || i == 2 ? FONT_BOLD : FONT_REGULAR;
            int px = i == 0 ? 28 : 19;
            int tw = fontTextWidth(face, px, lines[i]);
            fontDraw(face, px, (GFX_WIDTH - tw) / 2, 250 + i * 48 + (i ? 10 : 0), lines[i],
                     i == 2 ? COLOR_ACCENT : i == 0 ? COLOR_TEXT : COLOR_DIM);
        }
        int bw = 220, bx = (GFX_WIDTH - bw) / 2, by = 470;
        gfxFillRoundRect(bx, by, bw, 48, 24, COLOR_SURFACE_HI);
        fontDrawCentered(FONT_BOLD, 19, bx + bw / 2.0f, by + 24.0f, T("Look again"), COLOR_TEXT);
        uiHitAdd(bx, by, bw, 48, HIT_RESCAN, 0);
    } else {
        gfxSetClip(0, LIB_TOP - 8, GFX_WIDTH, FOOTER_Y - LIB_TOP + 8);
        uiHitClip(LIB_TOP - 8, FOOTER_Y);
        uiHitAdd(0, LIB_TOP - 8, GFX_WIDTH, FOOTER_Y - LIB_TOP + 8, HIT_LIBRARY, 0);
        int first_row = app->scroll / LIB_CELL_H;
        int rows_shown = (FOOTER_Y - LIB_TOP) / LIB_CELL_H + 2;
        for (int row = first_row; row < first_row + rows_shown; row++) {
            for (int col = 0; col < LIB_COLUMNS; col++) {
                int i = row * LIB_COLUMNS + col;
                if (i >= app->order_count)
                    break;
                int x = LIB_LEFT + col * LIB_CELL_W, y = LIB_TOP + row * LIB_CELL_H - app->scroll;
                drawBook(app, i, x, y, i == app->cursor && app->panel == PANEL_NONE);
            }
        }
        // The next row's covers are asked for before it shows.
        for (int col = 0; col < LIB_COLUMNS; col++) {
            int i = (first_row + rows_shown) * LIB_COLUMNS + col;
            if (i < app->order_count) {
                const ShelfBook *b = &app->shelf.items[app->order[i]];
                coversGet(&app->covers, b->path, b->size, (int)(fb(LIB_COVER_W) * gfxDetail()),
                          (int)(fb(LIB_COVER_H) * gfxDetail()), NULL);
            }
        }
        uiHitClip(0, GFX_HEIGHT);
        gfxResetClip();
    }
    gfxFillRect(0, FOOTER_Y, GFX_WIDTH, GFX_HEIGHT - FOOTER_Y, COLOR_BG);
    uiDrawHelp(MARGIN, FOOTER_Y + 28.0f, GFX_WIDTH - 2 * MARGIN,
               app->order_count ? T("[A] Read  [Y] Look again  [+] Menu") : T("[Y] Look again  [+] Menu"));
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

static void drawBars(const App *app, Color ink, Color paper)
{
    (void)ink;
    (void)paper;
    const ShelfBook *b = &app->shelf.items[app->book];
    gfxBlendRect(0, 0, GFX_WIDTH, BARS_TOP_H, COLOR_BG, 240);
    // Back to the library: an arrow.
    float cx = MARGIN + 10.0f, cy = 32.0f;
    gfxLine(cx + 6, cy - 10, cx - 4, cy, 3.0f, COLOR_TEXT);
    gfxLine(cx - 4, cy, cx + 6, cy + 10, 3.0f, COLOR_TEXT);
    uiHitAdd(0, 0, MARGIN + 40, BARS_TOP_H, HIT_BACK, 0);
    char name[SHELF_TEXT_MAX];
    shelfDisplayName(b, name, sizeof(name));
    int title_w = GFX_WIDTH - 2 * MARGIN - 320;
    fontDrawClipped(FONT_BOLD, 22, MARGIN + 40, 10, title_w, name, COLOR_TEXT);
    if (b->author[0])
        fontDrawClipped(FONT_REGULAR, 15, MARGIN + 40, 38, title_w, b->author, COLOR_DIM);
    uiDrawStatus(&app->sys, app->prefs.clock_24h, GFX_WIDTH - MARGIN, 18, COLOR_TEXT, COLOR_DIM);

    gfxBlendRect(0, BARS_BOTTOM_Y, GFX_WIDTH, GFX_HEIGHT - BARS_BOTTOM_Y, COLOR_BG, 240);
    int bw = 120;
    gfxFillRoundRect(MARGIN, BARS_BOTTOM_Y + 12, bw, 36, 18, COLOR_SURFACE_HI);
    fontDrawCentered(FONT_BOLD, 16, MARGIN + bw / 2.0f, BARS_BOTTOM_Y + 30.0f, T("Contents"), COLOR_TEXT);
    uiHitAdd(MARGIN, BARS_BOTTOM_Y + 6, bw, 48, HIT_CONTENTS, 0);

    int left = SLIDER_LEFT, width = GFX_WIDTH - 2 * left, sy = BARS_BOTTOM_Y + 30;
    gfxFillRoundRect(left, sy - 3, width, 6, 3, COLOR_SURFACE_HI);
    float t = app->pages > 1 ? (float)app->page / (app->pages - 1) : 0;
    int knob = left + (int)(t * width);
    gfxFillRoundRect(left, sy - 3, knob - left, 6, 3, COLOR_ACCENT);
    gfxFillCircle(knob, sy, 10, COLOR_ACCENT);
    uiHitAdd(left - 20, BARS_BOTTOM_Y + 4, width + 40, 52, HIT_SLIDER, 0);
    char where[64];
    snprintf(where, sizeof(where), T("%d of %d"), app->page + 1, app->pages);
    int ww = fontTextWidth(FONT_BOLD, 17, where);
    fontDraw(FONT_BOLD, 17, GFX_WIDTH - MARGIN - ww, BARS_BOTTOM_Y + 18, where, COLOR_TEXT);

    int ch = appChapterOf(app, app->page);
    if (ch >= 0)
        fontDrawClipped(FONT_REGULAR, 16, left, BARS_BOTTOM_Y + 50, width / 2, app->chapters[ch].title, COLOR_DIM);
    uiDrawHelp(GFX_WIDTH / 2, BARS_BOTTOM_Y + 62.0f, GFX_WIDTH / 2 - MARGIN,
               T("[Y] Colors  [^v] Text size  [-] Hide  [B] Library"));
}

static void drawReader(App *app, uint64_t now_ns)
{
    DocColors colors = prefsColors(app->prefs.theme);
    Color paper = uiColorOf(colors.paper), ink = uiColorOf(colors.ink);
    bool flowing = !app->ready || app->reflowable;
    gfxClear(flowing ? paper : GFX_RGB(0x0E, 0x0E, 0x10));
    if (!flowing)
        ink = COLOR_DIM;
    uiHitAdd(0, 0, GFX_WIDTH, GFX_HEIGHT, HIT_PAGE, 0);
    if (!app->ready) {
        uiDrawSpinner(GFX_WIDTH / 2.0f, GFX_HEIGHT / 2.0f - 20, 22, now_ns, ink);
        const char *text = T("Opening the book\xE2\x80\xA6");
        fontDrawCentered(FONT_REGULAR, 18, GFX_WIDTH / 2.0f, GFX_HEIGHT / 2.0f + 30, text, gfxMix(ink, paper, 80));
        return;
    }
    if (app->turn_from >= 0) {
        float t = (float)(now_ns - app->turn_start_ns) / READ_TURN_NS;
        t = t > 1 ? 1 : t;
        float eased = 1 - (1 - t) * (1 - t) * (1 - t);
        int shift = (int)(eased * GFX_WIDTH);
        drawPage(app, app->turn_from, -app->turn_dir * shift);
        drawPage(app, app->page, app->turn_dir * (GFX_WIDTH - shift));
    } else if (appPageImage(app, app->page)) {
        drawPage(app, app->page, 0);
    } else {
        uiDrawSpinner(GFX_WIDTH / 2.0f, GFX_HEIGHT / 2.0f, 18, now_ns, gfxMix(ink, paper, 120));
    }
    if (flowing && !app->bars) {
        // Under the page, faint: the chapter and where in the book.
        Color faint = gfxMix(ink, paper, 150);
        char where[32];
        snprintf(where, sizeof(where), "%d / %d", app->page + 1, app->pages);
        int x = (GFX_WIDTH - READ_COLUMN_W) / 2, y = READ_COLUMN_Y + READ_COLUMN_H + 8;
        int ww = fontTextWidth(FONT_REGULAR, 15, where);
        fontDraw(FONT_REGULAR, 15, x + READ_COLUMN_W - ww, y, where, faint);
        int ch = appChapterOf(app, app->page);
        if (ch >= 0)
            fontDrawClipped(FONT_REGULAR, 15, x, y, READ_COLUMN_W - ww - 40, app->chapters[ch].title, faint);
    }
    if (app->bars)
        drawBars(app, ink, paper);
}

// --- Panels ---

static void dim(void)
{
    gfxBlendRect(0, 0, GFX_WIDTH, GFX_HEIGHT, GFX_RGB(0, 0, 0), 150);
    uiHitAdd(0, 0, GFX_WIDTH, GFX_HEIGHT, HIT_OVERLAY_OUT, 0);
}

static void menuValue(const App *app, MenuItem item, char *out, size_t size)
{
    out[0] = 0;
    if (item == MENU_TEXT_SIZE)
        snprintf(out, size, T("%d of %d"), app->prefs.text_size + 1, PREFS_TEXT_SIZES);
    else if (item == MENU_THEME)
        snprintf(out, size, "%s", T(prefsThemeName(app->prefs.theme)));
    else if (item == MENU_CLOCK)
        snprintf(out, size, "%s", app->prefs.clock_24h ? T("24-hour") : T("12-hour"));
}

static const char *menuLabel(MenuItem item)
{
    switch (item) {
    case MENU_TEXT_SIZE:
        return T("Text size");
    case MENU_THEME:
        return T("Colors");
    case MENU_CONTENTS:
        return T("Contents");
    case MENU_LIBRARY:
        return T("Back to the library");
    case MENU_CLOCK:
        return T("Clock");
    case MENU_ABOUT:
        return T("About KaiReader");
    case MENU_EXIT:
        return T("Exit");
    default:
        return "";
    }
}

static void drawMenu(const App *app)
{
    dim();
    MenuItem rows[MENU_COUNT];
    int n = appMenuRows(app, rows);
    int h = 70 + n * MENU_ROW_H + 56, x = (GFX_WIDTH - MENU_W) / 2, y = (GFX_HEIGHT - h) / 2;
    gfxFillRoundRect(x, y, MENU_W, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, MENU_W, h, HIT_MENU_ITEM, -1);
    fontDraw(FONT_BOLD, 24, x + 28, y + 22, "KaiReader", COLOR_ACCENT);
    for (int i = 0; i < n; i++) {
        int ry = y + 70 + i * MENU_ROW_H;
        bool on = i == app->menu_cursor;
        if (on)
            gfxFillRoundRect(x + 12, ry, MENU_W - 24, MENU_ROW_H - 4, 12, COLOR_SURFACE_HI);
        fontDrawMiddle(FONT_BOLD, 19, x + 28, ry + (MENU_ROW_H - 4) / 2.0f, menuLabel(rows[i]),
                       on ? COLOR_ACCENT : COLOR_TEXT);
        char value[48];
        menuValue(app, rows[i], value, sizeof(value));
        if (value[0]) {
            int vw = fontTextWidth(FONT_REGULAR, 18, value);
            int right = x + MENU_W - 32;
            float mid = ry + (MENU_ROW_H - 4) / 2.0f;
            gfxFillTriangleRight(right - 10, (int)mid - 8, 10, 16, on ? COLOR_TEXT : COLOR_FAINT);
            fontDrawMiddle(FONT_REGULAR, 18, right - 22 - vw, mid, value, COLOR_TEXT);
            gfxFillTriangleLeft(right - 42 - vw, (int)mid - 8, 10, 16, on ? COLOR_TEXT : COLOR_FAINT);
            int half = (MENU_W - 24) / 2;
            uiHitAdd(x + 12, ry, half, MENU_ROW_H - 4, HIT_MENU_STEP, i * 2);
            uiHitAdd(x + 12 + half, ry, half, MENU_ROW_H - 4, HIT_MENU_STEP, i * 2 + 1);
        } else {
            uiHitAdd(x + 12, ry, MENU_W - 24, MENU_ROW_H - 4, HIT_MENU_ITEM, i);
        }
    }
    uiDrawHelp(x + 28, y + h - 30.0f, MENU_W - 56, T("[A] Choose  [<>] Change  [B] Close"));
}

static void drawContents(const App *app)
{
    dim();
    int rows = app->chapter_count < CONTENTS_ROWS ? app->chapter_count : CONTENTS_ROWS;
    int h = 70 + rows * CONTENTS_ROW_H + 50, x = (GFX_WIDTH - CONTENTS_W) / 2, y = (GFX_HEIGHT - h) / 2;
    gfxFillRoundRect(x, y, CONTENTS_W, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, CONTENTS_W, h, HIT_CHAPTER, -1);
    fontDraw(FONT_BOLD, 24, x + 28, y + 22, T("Contents"), COLOR_TEXT);
    int current = appChapterOf(app, app->page);
    for (int r = 0; r < CONTENTS_ROWS; r++) {
        int i = app->contents_first + r;
        if (i >= app->chapter_count)
            break;
        const DocChapter *c = &app->chapters[i];
        int ry = y + 70 + r * CONTENTS_ROW_H;
        bool on = i == app->contents_cursor;
        if (on)
            gfxFillRoundRect(x + 12, ry, CONTENTS_W - 24, CONTENTS_ROW_H - 4, 10, COLOR_SURFACE_HI);
        int indent = (c->level > 4 ? 4 : c->level) * 26;
        char page[16] = "";
        if (c->page >= 0)
            snprintf(page, sizeof(page), "%d", c->page + 1);
        int pw = fontTextWidth(FONT_REGULAR, 16, page);
        float mid = ry + (CONTENTS_ROW_H - 4) / 2.0f;
        Color color = i == current ? COLOR_ACCENT : c->page < 0 ? COLOR_FAINT : COLOR_TEXT;
        gfxSetClip(x + 28 + indent, ry, CONTENTS_W - 100 - indent - pw, CONTENTS_ROW_H);
        fontDrawMiddle(c->level ? FONT_REGULAR : FONT_BOLD, 18, x + 28 + indent, mid, c->title[0] ? c->title : "\xE2\x80\x94",
                       color);
        gfxResetClip();
        fontDrawMiddle(FONT_REGULAR, 16, x + CONTENTS_W - 32 - pw, mid, page, COLOR_DIM);
        uiHitAdd(x + 12, ry, CONTENTS_W - 24, CONTENTS_ROW_H - 4, HIT_CHAPTER, i);
    }
    if (app->chapter_count > CONTENTS_ROWS) {  // Where the rows shown are in the whole list.
        int track = CONTENTS_ROWS * CONTENTS_ROW_H, top = y + 70;
        int bar = track * CONTENTS_ROWS / app->chapter_count;
        int at = top + (track - bar) * app->contents_first / (app->chapter_count - CONTENTS_ROWS);
        gfxFillRoundRect(x + CONTENTS_W - 8, at, 4, bar, 2, COLOR_FAINT);
    }
    uiDrawHelp(x + 28, y + h - 28.0f, CONTENTS_W - 56, T("[A] Go  [<>] Page  [B] Close"));
}

static void drawAbout(void)
{
    dim();
    int w = 760, h = 360, x = (GFX_WIDTH - w) / 2, y = (GFX_HEIGHT - h) / 2;
    gfxFillRoundRect(x, y, w, h, PANEL_RADIUS, COLOR_SURFACE);
    uiHitAdd(x, y, w, h, HIT_MENU_ITEM, -1);
    fontDraw(FONT_BOLD, 28, x + 32, y + 26, "KaiReader " KAIREADER_VERSION, COLOR_ACCENT);
    const char *paragraphs[] = {
        T("A book reader for Switch 2 homebrew. Your books from sdmc:/books: EPUB, PDF, CBZ, FB2, MOBI, TXT and XPS."),
        T("Pages are drawn with MuPDF (Artifex Software), under the GNU AGPL 3.0. KaiReader is free software under the same license; its code is on GitHub (mizuiroame/kaireader)."),
        T("Books locked with DRM can't be opened: only books you can read anywhere."),
    };
    int ty = y + 84;
    for (int p = 0; p < 3; p++) {
        char lines[5][96];
        int n = wrapLines(FONT_REGULAR, 18, paragraphs[p], w - 64, lines, 5);
        for (int i = 0; i < n; i++, ty += 27)
            fontDraw(FONT_REGULAR, 18, x + 32, ty, lines[i], p == 0 ? COLOR_TEXT : COLOR_DIM);
        ty += 16;
    }
    uiDrawHelp(x + 32, y + h - 32.0f, w - 64, T("[B] Close"));
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
    else if (app->panel == PANEL_CONTENTS)
        drawContents(app);
    else if (app->panel == PANEL_ABOUT)
        drawAbout();
    NoticeView views[NOTICE_STACK + 1];
    int n = noticeVisible(&app->notices, now_ns, views);
    uiDrawNotices(views, n);
}
