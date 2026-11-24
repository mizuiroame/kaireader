// Bookmarks on screen: the ribbon on a bookmarked page, the reading bar's
// bookmark button, and the Bookmarks tab of the contents panel.
#include "ui_marks.h"

#include <stdio.h>

#include "font.h"
#include "gfx.h"
#include "lang.h"
#include "ui_buttons.h"
#include "ui_common.h"
#include "ui_hit.h"

#define RIBBON_W 22
#define RIBBON_H 44

// A ribbon hanging from y: a band with a notch cut in its foot.
static void ribbon(float x, float y, float w, float h, Color color)
{
    gfxFillRect((int)x, (int)y, (int)w, (int)(h - w / 2), color);
    gfxTriangle(x, y + h - w / 2, x + w / 2, y + h - w / 2, x, y + h, color);
    gfxTriangle(x + w / 2, y + h - w / 2, x + w, y + h - w / 2, x + w, y + h, color);
}

void uiDrawPageRibbons(const App *app)
{
    if (app->turn_from >= 0)
        return;  // Not while a page slides.
    bool two = appTwoPages(app);
    for (int side = 0; side < (two ? 2 : 1); side++) {
        if (!appPageMarked(app, app->page + side))
            continue;
        float x;
        if (app->reflowable) {
            int cx, cy, cw, ch;
            appColumn(app, side, &cx, &cy, &cw, &ch);
            x = cx + cw - RIBBON_W - 6;
        } else {
            int w = gfxLayoutWidth();
            x = two ? (side ? w - RIBBON_W - 18 : w / 2 - RIBBON_W - 18) : w - RIBBON_W - 18;
        }
        ribbon(x, 0, RIBBON_W, RIBBON_H, COLOR_ACCENT);
    }
}

void uiDrawMarkButton(const App *app, int x, int y)
{
    bool on = appShownMarked(app);
    gfxFillRoundRect(x, y, MARK_BUTTON_W, 44, 22, COLOR_SURFACE_HI);
    float bx = x + MARK_BUTTON_W / 2.0f - 8, by = y + 10;
    if (on) {
        ribbon(bx, by, 16, 26, COLOR_ACCENT);
    } else {  // An outline: the page isn't bookmarked.
        gfxLine(bx, by, bx + 16, by, 2.0f, COLOR_TEXT);
        gfxLine(bx, by, bx, by + 26, 2.0f, COLOR_TEXT);
        gfxLine(bx + 16, by, bx + 16, by + 26, 2.0f, COLOR_TEXT);
        gfxLine(bx, by + 26, bx + 8, by + 18, 2.0f, COLOR_TEXT);
        gfxLine(bx + 16, by + 26, bx + 8, by + 18, 2.0f, COLOR_TEXT);
    }
    uiHitAdd(x - 4, 0, MARK_BUTTON_W + 8, 64, HIT_MARK_TOGGLE, 0);
}

void uiDrawContentsTabs(const App *app, int x, int y)
{
    char marks[64];
    snprintf(marks, sizeof(marks), app->book_mark_count ? "%s  %d" : "%s", T("Bookmarks"), app->book_mark_count);
    const char *names[2] = {T("Contents"), marks};
    int tx = x;
    for (int t = 0; t < 2; t++) {
        bool on = app->contents_tab == t;
        int tw = fontTextWidth(FONT_BOLD, 20, names[t]);
        fontDraw(FONT_BOLD, 20, tx, y, names[t], on ? COLOR_TEXT : COLOR_DIM);
        if (on)
            gfxFillRoundRect(tx, y + 32, tw, 3, 1, COLOR_ACCENT);
        uiHitAdd(tx - 10, y - 10, tw + 20, 50, HIT_TAB, t);
        tx += tw + 34;
    }
}

void uiDrawMarksList(const App *app, int x, int y, int w, int rows)
{
    if (app->book_mark_count == 0) {
        fontDrawClipped(FONT_REGULAR, 17, x + 16, y + 14, w - 32, T("No bookmarks in this book yet."), COLOR_TEXT);
        fontDrawClipped(FONT_REGULAR, 15, x + 16, y + 44, w - 32,
                        T("Press the left stick in, or the ribbon on the reading bar, to bookmark a page."), COLOR_DIM);
        return;
    }
    if (app->mark_found_count < 0) {
        fontDraw(FONT_REGULAR, 17, x + 16, y + 14, T("Finding your bookmarks\xE2\x80\xA6"), COLOR_DIM);
        return;
    }
    for (int r = 0; r < rows; r++) {
        int i = app->contents_first + r;
        if (i >= app->book_mark_count || i >= app->mark_found_count)
            break;
        const ReadingMark *m = &app->mark_found[i];
        int ry = y + r * CONTENTS_ROW_H;
        bool on = i == app->contents_cursor;
        if (on)
            gfxFillRoundRect(x, ry, w, CONTENTS_ROW_H - 4, 10, COLOR_SURFACE_HI);
        float mid = ry + (CONTENTS_ROW_H - 4) / 2.0f;
        char page[24];
        snprintf(page, sizeof(page), T("p. %d"), m->page + 1);
        int pw = fontTextWidth(FONT_REGULAR, 16, page);
        bool here = m->page == app->page || (appTwoPages(app) && m->page == app->page + 1);
        ribbon(x + 14, ry + 10, 10, 20, here ? COLOR_ACCENT : COLOR_DIM);
        gfxSetClip(x + 36, ry, w - 60 - pw, CONTENTS_ROW_H);
        fontDrawMiddle(FONT_REGULAR, 17, x + 36, mid, m->text[0] ? m->text : T("(a page without text)"),
                       here ? COLOR_ACCENT : COLOR_TEXT);
        gfxResetClip();
        fontDrawMiddle(FONT_REGULAR, 16, x + w - 16 - pw, mid, page, COLOR_DIM);
        uiHitAdd(x, ry, w, CONTENTS_ROW_H - 4, HIT_CHAPTER, i);
    }
}
