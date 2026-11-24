// Renders KaiReader's screens on the PC with the made-up books (make_fixtures.py
// --library), through the real reading thread and MuPDF, lying down and
// upright, and checks that every touch target can be reached (none hidden
// under another). Problems make it fail.
//   run from tests/preview/build: ./preview <out folder>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "buttons.h"
#include "font.h"
#include "gfx.h"
#include "reading.h"
#include "ui.h"
#include "ui_hit.h"

static uint32_t g_pixels[GFX_FB_WIDTH * GFX_FB_HEIGHT];  // Also 1080 x 1920 upright: the same count.
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
    int w = gfxFbWidth(), h = gfxFbHeight();
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = g_pixels[i];
        unsigned char rgb[3] = {p & 0xFF, (p >> 8) & 0xFF, (p >> 16) & 0xFF};
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) == 0;
}

static void draw(App *app, uint64_t now)
{
    gfxSetTarget(g_pixels, (uint32_t)gfxFbWidth());
    uiDraw(app, now);
    gfxSetTarget(NULL, 0);
}

static bool isBackground(const HitRegion *r)
{
    return r->kind == HIT_LIBRARY || r->kind == HIT_PAGE || r->kind == HIT_OVERLAY_OUT ||
           ((r->kind == HIT_MENU_ITEM || r->kind == HIT_CHAPTER || r->kind == HIT_SETTING) && r->arg < 0);
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
        if (r->w <= 0 || r->h <= 0 || isBackground(r))
            continue;
        if (r->x < 0 || r->y < 0 || r->x + r->w > gfxLayoutWidth() + 1 || r->y + r->h > gfxLayoutHeight() + 1) {
            printf("TOUCH: %s: kind %d arg %d at %d,%d %dx%d goes off the screen\n", name, r->kind, r->arg, r->x, r->y,
                   r->w, r->h);
            g_problems++;
        }
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
    app->notices = (Notices){0};  // Notices from the steps before aren't part of the shots.
    draw(app, 1234567890ull);
    checkTouch(name);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    if (!writePpm(path))
        fprintf(stderr, "cannot write %s\n", path);
}

// Frames until the book is open and its page is here, and the covers shown are made (or 20 s).
static void settle(App *app)
{
    uint64_t now = 2000000000ull;
    for (int i = 0; i < 4000; i++, now += 16000000ull) {
        appUpdate(app, now);
        draw(app, now);  // Drawing asks for covers.
        bool page = app->view != VIEW_READER || (app->ready && appPageImage(app, app->page));
        bool covers = true;
        for (int b = 0; b < app->order_count && b < 18 && app->view == VIEW_LIBRARY; b++) {
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

static void setSort(App *app, ShelfSort sort)
{
    // As Y does it: one step at a time, until the sort asked for.
    for (int i = 0; i < SHELF_SORT_COUNT && app->prefs.sort != (int)sort; i++)
        appHandleButtons(app, HidNpadButton_Y, 5);
    settle(app);
}

static void settingsShots(App *app, const char *dir, const char *prefix)
{
    char name[64];
    app->panel = PANEL_SETTINGS;
    app->settings_page = SETTINGS_PAGE_MAIN;
    app->settings_cursor = 0;
    snprintf(name, sizeof(name), "%ssettings", prefix);
    render(app, dir, name);
    static const SettingsPage PAGES[] = {SETTINGS_PAGE_READING, SETTINGS_PAGE_LIBRARY, SETTINGS_PAGE_DISPLAY,
                                         SETTINGS_PAGE_ABOUT};
    static const char *const NAMES[] = {"reading", "library", "display", "about"};
    for (int i = 0; i < 4; i++) {
        app->settings_page = PAGES[i];
        app->settings_cursor = 0;
        snprintf(name, sizeof(name), "%ssettings-%s", prefix, NAMES[i]);
        render(app, dir, name);
    }
    app->panel = PANEL_NONE;
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
    // Some books read, so the library shows progress.
    static const char *const READ[] = {"library1.epub", "library4.epub", "quiet.epub"};
    static const int PERCENT[] = {38, 72, 12};
    for (int i = 0; i < 3; i++) {
        int b = indexOfBook(&app, READ[i]);
        if (b >= 0)
            shelfMarkRead(&app.shelf.items[app.order[b]], (DocPlace){0, 0.1}, PERCENT[i], 1000 + i);
    }
    setSort(&app, SHELF_SORT_TITLE);
    setSort(&app, SHELF_SORT_RECENT);
    render(&app, dir, "library");
    setSort(&app, SHELF_SORT_AUTHOR);
    render(&app, dir, "library-author");
    setSort(&app, SHELF_SORT_GENRE);
    render(&app, dir, "library-genre");
    setSort(&app, SHELF_SORT_RECENT);
    app.panel = PANEL_MENU;
    app.menu_cursor = 0;
    render(&app, dir, "menu-library");
    app.panel = PANEL_NONE;
    settingsShots(&app, dir, "");

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
    appHandleButtons(&app, HidNpadButton_Y, 3);  // Colors: Sepia.
    appHandleButtons(&app, HidNpadButton_Up, 4);  // A bigger text.
    appHandleButtons(&app, HidNpadButton_Up, 4);
    settle(&app);
    render(&app, dir, "epub-sepia-big");
    appHandleButtons(&app, HidNpadButton_Y, 5);  // Dark.
    settle(&app);
    render(&app, dir, "epub-dark");
    appHandleButtons(&app, HidNpadButton_X, 6);
    render(&app, dir, "contents");
    app.panel = PANEL_NONE;

    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "plain.pdf"), 7);
    settle(&app);
    render(&app, dir, "pdf");
    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "strip.cbz"), 8);
    settle(&app);
    render(&app, dir, "comic");
    appCloseBook(&app);
    settle(&app);

    // Upright: as Settings > Display > Screen does it.
    app.panel = PANEL_SETTINGS;
    app.settings_page = SETTINGS_PAGE_DISPLAY;
    app.settings_cursor = 0;
    appHandleButtons(&app, HidNpadButton_A, 9);  // Screen: Upright, right side down.
    app.panel = PANEL_NONE;
    settle(&app);
    render(&app, dir, "upright-library");
    setSort(&app, SHELF_SORT_AUTHOR);
    render(&app, dir, "upright-library-author");
    setSort(&app, SHELF_SORT_RECENT);
    settingsShots(&app, dir, "upright-");
    appOpenBook(&app, indexOfBook(&app, "library2.epub"), 10);
    settle(&app);
    render(&app, dir, "upright-epub");
    app.bars = true;
    render(&app, dir, "upright-epub-bars");
    app.bars = false;
    appHandleButtons(&app, HidNpadButton_X, 11);
    render(&app, dir, "upright-contents");
    app.panel = PANEL_NONE;
    appCloseBook(&app);
    app.prefs.turn = GFX_TURN_NONE;  // Lying down again for the next run's settings.
    gfxSetTurn(GFX_TURN_NONE);

    appFree(&app);
    readingStop();
    fontExit();
    printf("%d problems\n", g_problems);
    return g_problems ? 1 : 0;
}
