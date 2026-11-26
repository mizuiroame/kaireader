#include "ui_net.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "copybar.h"
#include "davnet.h"
#include "font.h"
#include "gfx.h"
#include "lang.h"
#include "ui_buttons.h"
#include "ui_common.h"
#include "ui_hit.h"

#define PILL_Y 14  // The header's pills, as KaiPlayer's.
#define PILL_H 30
#define PILL_GAP 16
#define SOURCE_DOT 10
#define COPY_W 620
#define COPY_H 230
#define BAR_H 8

// Each source's name, as the pill and the help line say it, and its color.
static const char *const SOURCE_NAMES[LIB_SOURCES] = {N_("SD card"), N_("Network folder"), N_("Book server"),
                                                       N_("Free books"), N_("News")};

const char *uiSourceName(int tab)
{
    return T(SOURCE_NAMES[tab >= 0 && tab < LIB_SOURCES ? tab : 0]);
}

Color uiSourceColor(int tab)
{
    return tab == LIB_NETWORK ? COLOR_NETWORK : tab == LIB_SERVER ? COLOR_SERVER : tab == LIB_FREE ? COLOR_FREE
           : tab == LIB_NEWS ? COLOR_NEWS : COLOR_ACCENT;
}

int uiDrawSourcePill(const App *app)
{
    const char *label = uiSourceName(app->lib_tab);
    bool key = app->panel == PANEL_NONE;  // X switches it while no panel is open.
    int key_w = key ? uiButtonWidth("X") + 8 : 0;
    const char *what = T("Source:");  // As the other Kai apps: "Source: SD card".
    int what_w = fontTextWidth(FONT_REGULAR, 16, what) + 8;
    int x = MARGIN + 34 + fontTextWidth(FONT_BOLD, 26, "KaiReader") + PILL_GAP;
    int w = 12 + key_w + what_w + SOURCE_DOT + 8 + fontTextWidth(FONT_BOLD, 16, label) + 14;
    float cy = PILL_Y + PILL_H / 2.0f;
    gfxFillRoundRect(x, PILL_Y, w, PILL_H, PILL_H / 2, COLOR_SURFACE_HI);
    if (key)
        uiDrawButton(x + 8, cy, "X", COLOR_SURFACE, COLOR_TEXT);
    fontDrawMiddle(FONT_REGULAR, 16, x + 12 + key_w, cy, what, COLOR_DIM);
    int dot_x = x + 12 + key_w + what_w;
    gfxFillRoundRect(dot_x, (int)cy - SOURCE_DOT / 2, SOURCE_DOT, SOURCE_DOT, SOURCE_DOT / 2,
                     uiSourceColor(app->lib_tab));
    fontDrawMiddle(FONT_BOLD, 16, dot_x + SOURCE_DOT + 8, cy, label, COLOR_TEXT);
    uiHitAdd(x - 4, 4, w + 8, HEADER_H - 8, HIT_SOURCE, 0);
    return x + w;
}

void uiDrawCoversPill(int left, int right, uint64_t now_ns, int tab)
{
    const char *label = T("Loading covers");
    int w = 12 + 18 + 8 + fontTextWidth(FONT_REGULAR, 16, label) + 14, x = right - w;
    if (x < left + PILL_GAP) {  // No room for the words: the spinner alone.
        w = 42;
        x = right - w;
        label = "";
    }
    float cy = PILL_Y + PILL_H / 2.0f;
    gfxFillRoundRect(x, PILL_Y, w, PILL_H, PILL_H / 2, COLOR_SURFACE_HI);
    uiDrawSpinner(x + 21.0f, cy, 8.0f, now_ns, uiSourceColor(tab));
    fontDrawMiddle(FONT_REGULAR, 16, x + 38, cy, label, COLOR_DIM);
}

void uiDrawNetScan(int tab, int x, float cy, uint64_t now_ns)
{
    int found;
    if (tab == LIB_FREE) {  // A list's page being fetched.
        if (!appFreeLoading())
            return;
        uiDrawSpinner(x + 10.0f, cy, 9.0f, now_ns, COLOR_FREE);
        fontDrawMiddle(FONT_REGULAR, 16, x + 28, cy, T("Loading..."), COLOR_DIM);
        return;
    }
    if (tab == LIB_NEWS) {  // The feeds being checked, one after another.
        int done, count;
        if (!appNewsChecking(&done, &count))
            return;
        char text[96];
        snprintf(text, sizeof(text), T("Checking the feeds... %d of %d"), done + 1 > count ? count : done + 1, count);
        uiDrawSpinner(x + 10.0f, cy, 9.0f, now_ns, COLOR_NEWS);
        fontDrawMiddle(FONT_REGULAR, 16, x + 28, cy, text, COLOR_DIM);
        return;
    }
    if (tab == LIB_NETWORK ? !appNetScanning(&found) : tab == LIB_SERVER ? !appServerScanning(&found) : true)
        return;
    uiDrawSpinner(x + 10.0f, cy, 9.0f, now_ns, uiSourceColor(tab));
    char text[96];
    snprintf(text, sizeof(text), tab == LIB_SERVER ? T("Looking through the catalog... %d") : T("Looking through the folder... %d"),
             found);
    fontDrawMiddle(FONT_REGULAR, 16, x + 28, cy, text, COLOR_DIM);
}

// A few centered lines (the first bold) in the library's room, and maybe a button under them.
static void drawMessage(const char *const *lines, int count, const char *button, HitKind hit)
{
    int w = gfxLayoutWidth(), y = LIB_TOP + 90;
    for (int i = 0; i < count; i++, y += i == 1 ? 46 : 32) {
        FontFace face = i == 0 ? FONT_BOLD : FONT_REGULAR;
        int px = i == 0 ? 26 : 18, tw = fontTextWidth(face, px, lines[i]);
        int max = w - 2 * MARGIN;
        fontDrawClipped(face, px, tw < max ? (w - tw) / 2 : MARGIN, y, max, lines[i], i == 0 ? COLOR_TEXT : COLOR_DIM);
    }
    if (!button)
        return;
    int bw = fontTextWidth(FONT_BOLD, 19, button) + 64, bx = (w - bw) / 2, by = y + 12;
    gfxFillRoundRect(bx, by, bw, 48, 24, COLOR_SURFACE_HI);
    fontDrawCentered(FONT_BOLD, 19, bx + bw / 2.0f, by + 24.0f, button, COLOR_TEXT);
    uiHitAdd(bx, by, bw, 48, hit, 0);
}

// The book server's library with no books.
static void drawServerEmpty(const App *app, uint64_t now_ns)
{
    int found;
    if (appServerScanning(&found)) {
        uiDrawSpinner(gfxLayoutWidth() / 2.0f, LIB_TOP + 60.0f, 16.0f, now_ns, COLOR_SERVER);
        const char *lines[] = {T("Looking through the server's catalog..."),
                               T("Its books show here, with their covers, as they're found.")};
        drawMessage(lines, 2, NULL, HIT_RESCAN_NET);
    } else if (app->server_problem[0]) {
        const char *lines[] = {T("The book server couldn't be read."), app->server_problem};
        drawMessage(lines, 2, T("Try again"), HIT_RESCAN_NET);
    } else if (app->prefs.sort == SHELF_SORT_FAVORITES) {
        const char *lines[] = {T("No favorites here yet."),
                               T("Hold your finger on a cover, or choose \"Add to favorites\" in the + menu.")};
        drawMessage(lines, 2, NULL, HIT_RESCAN_NET);
    } else {
        const char *lines[] = {T("No books in the server's catalog."),
                               T("KaiReader reads its EPUB, PDF, CBZ, CBR, FB2 and MOBI books.")};
        drawMessage(lines, 2, T("Look again"), HIT_RESCAN_NET);
    }
}

// Free books with nothing shown: loading, why not, or nothing found.
static void drawFreeEmpty(const App *app, uint64_t now_ns)
{
    if (appFreeLoading()) {
        uiDrawSpinner(gfxLayoutWidth() / 2.0f, LIB_TOP + 60.0f, 16.0f, now_ns, COLOR_FREE);
        const char *lines[] = {appFreeListName(app), T("Public-domain books, free to read. Loading the list...")};
        drawMessage(lines, 2, NULL, HIT_RESCAN_NET);
    } else if (app->free_problem[0]) {
        const char *lines[] = {T("The catalog couldn't be read."), app->free_problem};
        drawMessage(lines, 2, T("Try again"), HIT_RESCAN_NET);
    } else {
        char line[256];
        snprintf(line, sizeof(line), T("Nothing found for \"%s\"."), app->free_query);
        const char *lines[] = {line, appFreeSearchHint(app)};
        drawMessage(lines, 2, T("Search again"), HIT_SORT);
    }
}

// The news with no articles shown: being checked, why not, or none yet.
static void drawNewsEmpty(const App *app, uint64_t now_ns)
{
    int done, count;
    if (appNewsChecking(&done, &count)) {
        uiDrawSpinner(gfxLayoutWidth() / 2.0f, LIB_TOP + 60.0f, 16.0f, now_ns, COLOR_NEWS);
        const char *lines[] = {T("Checking the feeds..."), T("Their newest articles show here once they come.")};
        drawMessage(lines, 2, NULL, HIT_RESCAN_NET);
    } else if (app->news_problem[0]) {
        const char *lines[] = {T("The feeds couldn't be read."), app->news_problem};
        drawMessage(lines, 2, T("Try again"), HIT_RESCAN_NET);
    } else {
        const char *lines[] = {T("No articles here yet."), T("Y checks the feeds again; Settings > News adds more.")};
        drawMessage(lines, 2, T("Check again"), HIT_RESCAN_NET);
    }
}

void uiDrawNetEmpty(const App *app, uint64_t now_ns)
{
    if (app->lib_tab == LIB_NEWS) {
        drawNewsEmpty(app, now_ns);
        return;
    }
    if (app->lib_tab == LIB_FREE) {
        drawFreeEmpty(app, now_ns);
        return;
    }
    if (app->lib_tab == LIB_SERVER) {
        drawServerEmpty(app, now_ns);
        return;
    }
    int found;
    if (appNetScanning(&found)) {
        uiDrawSpinner(gfxLayoutWidth() / 2.0f, LIB_TOP + 60.0f, 16.0f, now_ns, COLOR_NETWORK);
        const char *lines[] = {T("Looking through the network folder..."),
                               T("Its books show here, with their covers, as they're found.")};
        drawMessage(lines, 2, NULL, HIT_RESCAN_NET);
    } else if (app->net_problem[0]) {
        const char *lines[] = {T("The network folder couldn't be read."), app->net_problem};
        drawMessage(lines, 2, T("Try again"), HIT_RESCAN_NET);
    } else if (app->prefs.sort == SHELF_SORT_FAVORITES) {
        const char *lines[] = {T("No favorites here yet."),
                               T("Hold your finger on a cover, or choose \"Add to favorites\" in the + menu.")};
        drawMessage(lines, 2, NULL, HIT_RESCAN_NET);
    } else {
        const char *lines[] = {T("No books in the network folder."),
                               T("Books are EPUB, PDF, CBZ, CBR, FB2, MOBI or TXT files, in it or its folders.")};
        drawMessage(lines, 2, T("Look again"), HIT_RESCAN_NET);
    }
}

// "1.2 MB", "340 KB".
static void sizeText(int64_t bytes, char *out, size_t size)
{
    if (bytes < 0)
        snprintf(out, size, "%s", "");
    else if (bytes >= 1024 * 1024)
        snprintf(out, size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else
        snprintf(out, size, "%" PRId64 " KB", (bytes + 1023) / 1024);
}

void uiDrawNetCopy(const App *app, uint64_t now_ns)
{
    if (!app->net_copying[0])
        return;
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxBlendRect(0, 0, w, h, GFX_RGB(0, 0, 0), 150);
    uiHitAdd(0, 0, w, h, HIT_OVERLAY_OUT, 0);  // What's under it can't be touched meanwhile.
    int pw = COPY_W < w - 40 ? COPY_W : w - 40, x = (w - pw) / 2, y = (h - COPY_H) / 2;
    gfxFillRoundRect(x, y, pw, COPY_H, 18, COLOR_SURFACE);
    uiHitAdd(x, y, pw, COPY_H, HIT_SETTING, -1);  // The panel itself: nothing.
    fontDraw(FONT_BOLD, 22, x + 28, y + 26, T("Copying to the card"), COLOR_TEXT);
    fontDrawClipped(FONT_REGULAR, 18, x + 28, y + 70, pw - 56, app->net_copying, COLOR_DIM);
    int64_t done, total;
    appNetCopyProgress(&done, &total);
    int bx = x + 28, by = y + 118, bw = pw - 56;
    gfxFillRoundRect(bx, by, bw, BAR_H, BAR_H / 2, COLOR_SURFACE_HI);
    CopyBar bar = copyBarOf(done, total, now_ns);  // Without a size: a piece that slides.
    int fx = bx + (int)(bw * bar.from), fw = (int)(bw * bar.to) - (int)(bw * bar.from);
    if (fw > 0)
        gfxFillRoundRect(fx, by, fw > BAR_H ? fw : BAR_H, BAR_H, BAR_H / 2, COLOR_NETWORK);
    char a[32], b[32], line[96];
    sizeText(done, a, sizeof(a));
    sizeText(total, b, sizeof(b));
    if (total > 0)
        snprintf(line, sizeof(line), T("%s of %s"), a, b);
    else
        snprintf(line, sizeof(line), "%s", a);
    fontDraw(FONT_REGULAR, 16, bx, by + 22, line, COLOR_DIM);
    uiDrawHelp(x + 28, y + COPY_H - 30.0f, pw - 56, T("[B] Stop"));
    int sw = fontTextWidth(FONT_BOLD, 16, T("Stop")) + 40, sx = x + pw - 28 - sw, sy = y + 20;
    gfxFillRoundRect(sx, sy, sw, 36, 18, COLOR_SURFACE_HI);
    fontDrawCentered(FONT_BOLD, 16, sx + sw / 2.0f, sy + 18.0f, T("Stop"), COLOR_TEXT);
    uiHitAdd(sx, sy, sw, 36, HIT_NET_CANCEL, 0);
}
