#include "ui_net.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "davnet.h"
#include "font.h"
#include "gfx.h"
#include "lang.h"
#include "ui_buttons.h"
#include "ui_common.h"
#include "ui_hit.h"

#define TAB_GAP 28
#define HINT_GAP 14
#define TAB_TEXT_MIDDLE 28.5f  // Middle of the tab names' capitals (as KaiPlayer's).
#define FOLDER_W 30
#define FOLDER_H 22
#define BADGE_W 54
#define COPY_W 620
#define COPY_H 230
#define BAR_H 8

static const char *const TAB_NAMES[2] = {N_("Library"), N_("Network")};

void uiDrawLibTabs(const App *app)
{
    int widths[2], total = 0;
    for (int i = 0; i < 2; i++) {
        widths[i] = fontTextWidth(app->lib_tab == i ? FONT_BOLD : FONT_REGULAR, 20, T(TAB_NAMES[i]));
        total += widths[i] + (i > 0 ? TAB_GAP : 0);
    }
    int x = (gfxLayoutWidth() - total) / 2;
    uiDrawButton(x - HINT_GAP - uiButtonWidth("ZL"), TAB_TEXT_MIDDLE, "ZL", COLOR_SURFACE_HI, COLOR_DIM);
    for (int i = 0; i < 2; i++) {
        bool active = app->lib_tab == i;
        fontDraw(active ? FONT_BOLD : FONT_REGULAR, 20, x, 18, T(TAB_NAMES[i]), active ? COLOR_TEXT : COLOR_DIM);
        uiHitAdd(x - TAB_GAP / 2, 4, widths[i] + TAB_GAP, HEADER_H - 8, HIT_LIB_TAB, i);
        if (active)
            gfxFillRoundRect(x, 48, widths[i], 3, 1, COLOR_ACCENT);
        x += widths[i] + TAB_GAP;
    }
    uiDrawButton(x - TAB_GAP + HINT_GAP, TAB_TEXT_MIDDLE, "ZR", COLOR_SURFACE_HI, COLOR_DIM);
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

static void drawFolderIcon(int x, float cy, Color color)
{
    int y = (int)(cy - FOLDER_H / 2.0f);
    gfxFillRoundRect(x, y - 3, FOLDER_W / 2, 8, 2, color);  // Its tab.
    gfxFillRoundRect(x, y, FOLDER_W, FOLDER_H, 3, color);
}

// A book's kind, as a small badge: "EPUB", "PDF", "CBR".
static void drawBadge(int x, float cy, const char *name)
{
    const char *dot = strrchr(name, '.');
    char ext[8] = "";
    for (int i = 0; dot && dot[i + 1] && i < 6; i++)
        ext[i] = (char)(dot[i + 1] >= 'a' && dot[i + 1] <= 'z' ? dot[i + 1] - 'a' + 'A' : dot[i + 1]);
    gfxFillRoundRect(x - 12, (int)cy - 13, BADGE_W, 26, 6, COLOR_SURFACE_HI);
    fontDrawCentered(FONT_BOLD, 13, x - 12 + BADGE_W / 2.0f, cy, ext, COLOR_ACCENT);
}

static void drawRow(const App *app, int i, int y, int w)
{
    const DavEntry *e = &app->net_entries[i];
    bool on = i == app->net_cursor;
    float mid = y + (NET_ROW_H - 4) / 2.0f;
    if (on) {
        gfxFillRoundRect(MARGIN - 12, y, w - 2 * MARGIN + 24, NET_ROW_H - 4, 10, COLOR_SURFACE_HI);
        gfxFillRect(MARGIN - 12, y + 14, 3, NET_ROW_H - 32, COLOR_ACCENT);
    }
    if (e->is_dir)
        drawFolderIcon(MARGIN + 8, mid + 2, on ? COLOR_ACCENT : COLOR_DIM);
    else
        drawBadge(MARGIN + 8, mid, e->name);
    char size[32];
    sizeText(e->is_dir ? -1 : e->size, size, sizeof(size));
    int sw = size[0] ? fontTextWidth(FONT_REGULAR, 16, size) : 0;
    int nx = MARGIN + 8 + BADGE_W + 8;
    fontDrawClipped(on ? FONT_BOLD : FONT_REGULAR, 19, nx, (int)mid - 12, w - MARGIN - nx - sw - 24, e->name,
                    COLOR_TEXT);
    if (sw)
        fontDrawMiddle(FONT_REGULAR, 16, w - MARGIN - sw, mid, size, COLOR_DIM);
    if (e->is_dir)
        gfxFillTriangleRight(w - MARGIN - 8, (int)mid - 7, 8, 14, on ? COLOR_TEXT : COLOR_FAINT);
    uiHitAdd(MARGIN - 12, y, w - 2 * MARGIN + 24, NET_ROW_H - 4, HIT_NET_ROW, i);
}

// A few centered lines (the first bold) in the list's room, and maybe a button under them.
static void drawMessage(const char *const *lines, int count, const char *button, HitKind hit)
{
    int w = gfxLayoutWidth(), y = NET_LIST_TOP + 90;
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

// The folder's place: "nas/Books › Comics".
static void drawPlace(const App *app, uint64_t now_ns)
{
    int w = gfxLayoutWidth();
    int tw = fontDraw(FONT_BOLD, 22, MARGIN, 74, T("Network folder"), COLOR_TEXT);
    DavAccount a;
    davGetAccount(&a);
    char shown[DAV_ADDRESS_MAX], place[DAV_ADDRESS_MAX * 2];
    davShownAddress(a.address, shown, sizeof(shown));
    memset(&a, 0, sizeof(a));
    const char *rest = app->net_location[0] ? app->net_location + strlen(DAV_PREFIX) : "";
    snprintf(place, sizeof(place), "%s%s%s", shown, rest[0] ? " / " : "", rest);
    int right = w - MARGIN - (app->net_loading ? 40 : 0);
    fontDrawClipped(FONT_REGULAR, 18, MARGIN + tw + 14, 77, right - (MARGIN + tw + 14), place, COLOR_DIM);
    if (app->net_loading)
        uiDrawSpinner(w - MARGIN - 12.0f, 88.0f, 11.0f, now_ns, COLOR_ACCENT);
}

static void drawCopy(const App *app)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxBlendRect(0, 0, w, h, GFX_RGB(0, 0, 0), 150);
    uiHitAdd(0, 0, w, h, HIT_OVERLAY_OUT, 0);  // What's under it can't be touched meanwhile.
    int pw = COPY_W < w - 40 ? COPY_W : w - 40, x = (w - pw) / 2, y = (h - COPY_H) / 2;
    gfxFillRoundRect(x, y, pw, COPY_H, 18, COLOR_SURFACE);
    uiHitAdd(x, y, pw, COPY_H, HIT_SETTING, -1);  // The panel itself: nothing.
    fontDraw(FONT_BOLD, 22, x + 28, y + 26, T("Copying to the card"), COLOR_TEXT);
    fontDrawClipped(FONT_REGULAR, 18, x + 28, y + 70, pw - 56, app->net_copying, COLOR_DIM);
    int64_t done, total;
    float share = appNetCopyProgress(&done, &total);
    int bx = x + 28, by = y + 118, bw = pw - 56;
    gfxFillRoundRect(bx, by, bw, BAR_H, BAR_H / 2, COLOR_SURFACE_HI);
    if (share >= 0)
        gfxFillRoundRect(bx, by, (int)(bw * (share > 1 ? 1 : share)), BAR_H, BAR_H / 2, COLOR_ACCENT);
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

void uiDrawNetwork(App *app, uint64_t now_ns)
{
    int w = gfxLayoutWidth(), h = gfxLayoutHeight();
    gfxClear(COLOR_BG);
    uiDrawHeader("KaiReader");
    uiDrawLibTabs(app);
    uiDrawStatus(&app->sys, app->prefs.clock_24h, w - MARGIN, 18, COLOR_TEXT, COLOR_DIM);
    if (!davConfigured()) {
        const char *lines[] = {
            T("No network folder yet."),
            T("Read the books of a shared folder, a NAS (WebDAV), or an FTP or SFTP server on your network."),
            T("A book opened is copied to sdmc:/books/Network, and stays in the library."),
        };
        drawMessage(lines, 3, T("Set it up"), HIT_NET_SETUP);
        gfxFillRect(0, h - FOOTER_H, w, FOOTER_H, COLOR_BG);
        uiDrawHelp(MARGIN, h - FOOTER_H + 28.0f, w - 2 * MARGIN, T("[ZL] Library  [+] Menu"));
        return;
    }
    drawPlace(app, now_ns);
    if (app->net_count > 0) {
        int rows = appNetRows();
        uiHitAdd(0, NET_LIST_TOP, w, rows * NET_ROW_H, HIT_NET_LIST, 0);
        for (int r = 0; r < rows && app->net_first + r < (int)app->net_count; r++)
            drawRow(app, app->net_first + r, NET_LIST_TOP + r * NET_ROW_H, w);
        if ((int)app->net_count > rows) {  // Where in the list: a thin bar at the right.
            int track = rows * NET_ROW_H - 8, bar = track * rows / (int)app->net_count;
            bar = bar < 24 ? 24 : bar;
            int by = NET_LIST_TOP + (track - bar) * app->net_first / ((int)app->net_count - rows);
            gfxFillRoundRect(w - 10, by, 4, bar, 2, COLOR_FAINT);
        }
    } else if (app->net_problem[0]) {
        const char *lines[] = {T("The folder couldn't be read."), app->net_problem};
        drawMessage(lines, 2, T("Try again"), HIT_RESCAN_NET);
    } else if (app->net_location[0] && !app->net_loading) {
        const char *lines[] = {T("No folders or books here."), T("Books are EPUB, PDF, CBZ, CBR, FB2, MOBI or TXT files.")};
        drawMessage(lines, 2, NULL, HIT_NET_LIST);
    }
    gfxFillRect(0, h - FOOTER_H, w, FOOTER_H, COLOR_BG);
    uiDrawHelp(MARGIN, h - FOOTER_H + 28.0f, w - 2 * MARGIN, T("[A] Open  [B] Back  [Y] Look again  [+] Menu"));
    if (app->net_copying[0])
        drawCopy(app);
}
