// A news article read as a web page, not a book: its text laid out at the
// column's width in strips a screen tall (the reading thread's pages, with
// no margin between them), drawn one under the other and scrolled. The
// reading thread keeps rendering only the strips near the one at the top
// (PAGE_SLOTS of them at most), so a long article costs no more than a book.
// A finger drags it (gliding on, as the library does), up and down scroll it
// (held: on and on), and L, R, ZL, ZR, left, right and A move a screen.
#include "app.h"

#include <stdlib.h>
#include <string.h>

#include "buttons.h"
#include "gfx.h"
#include "kinetic.h"

#define SCROLL_TOP 40      // Room above the article's first line.
#define SCROLL_BOTTOM 80   // And under its last.
#define SCROLL_STEP 72     // Up or down, once (or each repeat while held).
#define SCREEN_OVERLAP 96  // A screen down keeps this much of the one before in view.
#define CONTENT_DIFF 40    // A pixel this far from the paper's color is ink (antialiasing included).

bool appScrollView(const App *app)
{
    return app->scroll_view && app->view == VIEW_READER;
}

int appScrollStrip(void)
{
    return gfxLayoutHeight();
}

// The article's height on screen: every strip but the last whole, the last to its text's end.
static int contentHeight(const App *app)
{
    if (app->pages <= 0)
        return 0;
    int last = app->scroll_end >= 0 ? app->scroll_end : appScrollStrip();
    return SCROLL_TOP + (app->pages - 1) * appScrollStrip() + last + SCROLL_BOTTOM;
}

static int maxScroll(const App *app)
{
    int max = contentHeight(app) - gfxLayoutHeight();
    return max < 0 ? 0 : max;
}

int appScrollStripY(const App *app, int page)
{
    return SCROLL_TOP + page * appScrollStrip() - (int)app->scroll_y;
}

// The strip at the screen's top is the page the reading thread reads around (and keeps as the place).
static void followStrip(App *app)
{
    int y = (int)app->scroll_y - SCROLL_TOP;
    int page = y > 0 ? y / appScrollStrip() : 0;
    app->page = page >= app->pages ? app->pages - 1 : page < 0 ? 0 : page;
}

void appScrollTo(App *app, float y)
{
    float max = (float)maxScroll(app);
    app->scroll_y = y < 0 ? 0 : y > max ? max : y;
    if (app->scroll_y <= 0 || app->scroll_y >= max)
        kineticStop(&app->kinetic);
    followStrip(app);
}

void appScrollBy(App *app, float dy)
{
    appScrollTo(app, app->scroll_y + dy);
}

void appScrollOpened(App *app, int start_page)
{
    app->scroll_end = -1;
    kineticStop(&app->kinetic);
    app->scroll_y = start_page > 0 ? (float)(SCROLL_TOP + start_page * appScrollStrip()) : 0;
    followStrip(app);
}

void appScrollTick(App *app, uint64_t now_ns)
{
    int step = kineticStep(&app->kinetic, now_ns);
    if (step)
        appScrollBy(app, (float)step);
}

// Whether a pixel (0xAABBGGRR) is ink on paper (0xRRGGBB).
static bool inked(uint32_t p, uint32_t paper)
{
    int dr = (int)(p & 0xFF) - (int)((paper >> 16) & 0xFF);
    int dg = (int)((p >> 8) & 0xFF) - (int)((paper >> 8) & 0xFF);
    int db = (int)((p >> 16) & 0xFF) - (int)(paper & 0xFF);
    return abs(dr) + abs(dg) + abs(db) > CONTENT_DIFF;
}

void appScrollKept(App *app, int page, const DocImage *image)
{
    if (!app->scroll_view || page != app->pages - 1 || !image->pixels || image->height <= 0)
        return;
    uint32_t paper = prefsColors(app->prefs.theme).paper;
    int row = image->height - 1;
    for (; row > 0; row--) {
        const uint32_t *line = image->pixels + (size_t)row * (size_t)image->width;
        bool ink = false;
        for (int x = 0; x < image->width && !ink; x += 2)
            ink = inked(line[x], paper);
        if (ink)
            break;
    }
    // Rows of the image to layout units: the image is the strip, a screen tall.
    app->scroll_end = (int)((int64_t)(row + 1) * appScrollStrip() / image->height);
    appScrollTo(app, app->scroll_y);  // The end may have come nearer.
}

bool appScrollButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    (void)now_ns;
    int screen = gfxLayoutHeight() - SCREEN_OVERLAP;
    uint64_t down = HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_A | BUTTONS_RIGHT;
    uint64_t up = HidNpadButton_L | HidNpadButton_ZL | BUTTONS_LEFT;
    kineticStop(&app->kinetic);
    if (pressed & BUTTONS_UP)
        appScrollBy(app, -SCROLL_STEP);
    else if (pressed & BUTTONS_DOWN)
        appScrollBy(app, SCROLL_STEP);
    else if (pressed & down)
        appScrollBy(app, (float)screen);
    else if (pressed & up)
        appScrollBy(app, (float)-screen);
    else
        return false;
    return true;
}

float appScrollFraction(const App *app)
{
    int max = maxScroll(app);
    return max > 0 ? app->scroll_y / (float)max : 1.0f;
}
