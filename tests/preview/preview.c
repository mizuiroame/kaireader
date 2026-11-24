// Renders KaiReader's screens on the PC with the made-up books (make_fixtures.py),
// through the real reading thread and MuPDF, and checks that every touch
// target can be reached (none hidden under another). Problems make it fail.
//   run from tests/preview/build: ./preview <out folder>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "font.h"
#include "gfx.h"
#include "reading.h"
#include "ui.h"
#include "ui_hit.h"

static uint32_t g_pixels[GFX_FB_WIDTH * GFX_FB_HEIGHT];
static int g_problems;

static void nap(void)
{
    struct timespec ts = {0, 5 * 1000 * 1000};
    nanosleep(&ts, NULL);
}

static bool writePpm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    fprintf(f, "P6\n%d %d\n255\n", GFX_FB_WIDTH, GFX_FB_HEIGHT);
    for (int i = 0; i < GFX_FB_WIDTH * GFX_FB_HEIGHT; i++) {
        uint32_t p = g_pixels[i];
        unsigned char rgb[3] = {p & 0xFF, (p >> 8) & 0xFF, (p >> 16) & 0xFF};
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) == 0;
}

static void draw(App *app, uint64_t now)
{
    gfxSetTarget(g_pixels, GFX_FB_WIDTH);
    uiDraw(app, now);
    gfxSetTarget(NULL, 0);
}

// Every target's middle must find that target (not one drawn over it). The
// ones under an open panel (before its dimming) and under notices don't count.
static void checkTouch(const char *name)
{
    int first = 0;
    for (int i = 0; i < uiHitCount(); i++)
        if (uiHitAt(i)->kind == HIT_OVERLAY_OUT)
            first = i;
    for (int i = first; i < uiHitCount(); i++) {
        const HitRegion *r = uiHitAt(i);
        if (r->w <= 0 || r->h <= 0)
            continue;
        if (r->kind == HIT_LIBRARY || r->kind == HIT_PAGE || r->kind == HIT_OVERLAY_OUT ||
            (r->kind == HIT_MENU_ITEM && r->arg < 0) || (r->kind == HIT_CHAPTER && r->arg < 0))
            continue;  // Backgrounds: meant to be under the rest.
        const HitRegion *found = uiHitFind(r->x + r->w / 2, r->y + r->h / 2);
        if (found && found->kind == HIT_NOTICE)
            continue;
        if (found != r && !(found && found->kind == r->kind && found->arg == r->arg)) {
            printf("TOUCH: %s: kind %d arg %d at %d,%d is covered by kind %d\n", name, r->kind, r->arg, r->x, r->y,
                   found ? (int)found->kind : -1);
            g_problems++;
        }
    }
}

static void render(App *app, const char *dir, const char *name)
{
    draw(app, 1234567890ull);
    checkTouch(name);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    if (!writePpm(path))
        fprintf(stderr, "cannot write %s\n", path);
}

// Frames until the book is open and its page is here (or 10 s).
static void settle(App *app)
{
    uint64_t now = 2000000000ull;
    for (int i = 0; i < 2000; i++, now += 16000000ull) {
        appUpdate(app, now);
        draw(app, now);  // Drawing asks for covers.
        bool page = app->view != VIEW_READER || (app->ready && appPageImage(app, app->page));
        bool covers = true;
        for (int b = 0; b < app->order_count && b < 12 && app->view == VIEW_LIBRARY; b++) {
            const ShelfBook *s = &app->shelf.items[app->order[b]];
            bool failed;
            if (!coversGet(&app->covers, s->path, s->size, 240, 360, &failed) && !failed)
                covers = false;
        }
        if (page && covers && i > 3)
            return;
        nap();
    }
    printf("settle: gave up waiting\n");
}

static int indexOfBook(App *app, const char *file)
{
    for (int i = 0; i < app->order_count; i++)
        if (strstr(app->shelf.items[app->order[i]].path, file))
            return i;
    return -1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    mkdir(dir, 0755);
    if (system("rm -rf /tmp/kaireader_preview && mkdir -p /tmp/kaireader_preview") != 0)
        return 1;
    if (!fontInit() || !readingStart())
        return 1;
    static App app;
    appInit(&app, "/tmp/kaireader_preview");  // Books from ./books (the fixtures).
    app.sys = (SysInfo){true, 21, 47, true, 76, false};
    settle(&app);
    render(&app, dir, "library");

    app.panel = PANEL_MENU;
    render(&app, dir, "menu-library");
    app.panel = PANEL_ABOUT;
    render(&app, dir, "about");
    app.panel = PANEL_NONE;

    appOpenBook(&app, indexOfBook(&app, "quiet.epub"), 1);
    draw(&app, 1);
    render(&app, dir, "opening");
    settle(&app);
    app.page = 3;
    settle(&app);
    render(&app, dir, "epub-light");
    app.bars = true;
    render(&app, dir, "epub-bars");
    app.bars = false;
    app.panel = PANEL_MENU;
    render(&app, dir, "menu-reader");
    app.panel = PANEL_NONE;
    // As the menu does it: Colors, one step right from Sepia is Dark (laid out again).
    app.prefs.theme = THEME_SEPIA;
    app.panel = PANEL_MENU;
    app.menu_cursor = 1;
    appHandleButtons(&app, 1ull << 14, 3);
    settle(&app);
    app.panel = PANEL_NONE;
    render(&app, dir, "epub-dark");
    app.prefs.text_size = 8;
    app.prefs.theme = THEME_SEPIA;
    app.panel = PANEL_MENU;
    app.menu_cursor = 0;
    appHandleButtons(&app, 1ull << 12, 4);  // Left: text size 8 -> 7, laid out again in sepia.
    settle(&app);
    app.panel = PANEL_NONE;
    render(&app, dir, "epub-sepia-big");
    app.panel = PANEL_CONTENTS;
    app.contents_cursor = 1;
    render(&app, dir, "contents");
    app.panel = PANEL_NONE;

    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "plain.pdf"), 5);
    settle(&app);
    render(&app, dir, "pdf");
    app.bars = true;
    render(&app, dir, "pdf-bars");
    app.bars = false;
    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "strip.cbz"), 6);
    settle(&app);
    render(&app, dir, "comic");
    appCloseBook(&app);
    settle(&app);
    render(&app, dir, "library-after");

    appFree(&app);
    readingStop();
    fontExit();
    printf("%d problems\n", g_problems);
    return g_problems ? 1 : 0;
}
