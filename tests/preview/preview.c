// Renders KaiReader's screens on the PC with the made-up books (make_fixtures.py
// --library), through the real reading thread and MuPDF, lying down and
// upright, and checks that every touch target can be reached (none hidden
// under another). Problems make it fail.
//   run from tests/preview/build: ./preview <out folder>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "background.h"
#include "theme.h"
#include "buttons.h"
#include "catalogs.h"
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

// Frames until the open book's bookmarks have their pages (or 20 s), and its current page is told.
static void waitMarks(App *app)
{
    uint64_t now = 3000000000ull;
    for (int i = 0; i < 4000; i++, now += 16000000ull) {
        appUpdate(app, now);
        ReadingStatus st;
        readingGetStatus(&st);
        if (app->mark_found_count >= 0 && st.current_page == app->page)
            return;
        struct timespec ts = {0, 5000000};
        nanosleep(&ts, NULL);
    }
}

// Frames until the book is open and its page is here, and the covers shown are made (or 20 s).
static void settle(App *app)
{
    uint64_t now = 2000000000ull;
    for (int i = 0; i < 4000; i++, now += 16000000ull) {
        appUpdate(app, now);
        draw(app, now);  // Drawing asks for covers.
        bool page = app->view != VIEW_READER ||
                    (app->ready && appPageCurrent(app, app->page) &&
                     (!appTwoPages(app) || app->page + 1 >= app->pages || appPageCurrent(app, app->page + 1)));
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
                                         SETTINGS_PAGE_ABOUT, SETTINGS_PAGE_CREDITS};
    static const char *const NAMES[] = {"reading", "library", "display", "about", "credits"};
    for (int i = 0; i < 5; i++) {
        app->settings_page = PAGES[i];
        app->settings_cursor = 0;
        snprintf(name, sizeof(name), "%ssettings-%s", prefix, NAMES[i]);
        render(app, dir, name);
    }
    app->panel = PANEL_NONE;
}

// Where row is on page (its cursor), so a shot stays right when a page's rows move; 0 if it isn't there.
static int rowAt(SettingsPage page, SettingRow row)
{
    SettingRow rows[SETTING_COUNT];
    int n = settingsRows(page, rows);
    for (int i = 0; i < n; i++)
        if (rows[i] == row)
            return i;
    printf("PREVIEW: setting %d isn't on page %d\n", (int)row, (int)page);
    g_problems++;
    return 0;
}

extern bool g_preview_net_configured;
extern bool g_preview_copy_unknown;
extern bool g_preview_free_down;
extern bool g_preview_server_configured;

// The + menu on each Free books list: its search names the list's own catalog, and the list line
// the one L/R goes to next. Checked, and drawn (free-menu-0...).
static void freeMenus(App *app, const char *dir)
{
    snprintf(app->free_query, sizeof(app->free_query), "%s", "lighthouse");  // A search done: it's a list too.
    for (int list = 0; list < CATALOG_COUNT; list++) {
        if (list == CATALOG_SEARCH)
            app->free_search_in = CATALOG_FRENCH;
        appFreeShow(app, list, 50);
        settle(app);
        CatalogList in = catalogSearchOf((CatalogList)list, (CatalogList)app->free_search_in);
        char brand[64];
        snprintf(brand, sizeof(brand), "%.*s", (int)strcspn(catalogName(in), ":"), catalogName(in));
        const char *next = catalogShowLine((CatalogList)((list + 1) % CATALOG_COUNT));
        if (!strstr(appFreeSearchLine(app), brand) || strcmp(appFreeNextListLine(app), next) != 0 ||
            (list == CATALOG_SEARCH && strcmp(appFreeListName(app), catalogSearchName(CATALOG_FRENCH)) != 0)) {
            printf("PREVIEW: on Free books list %d the menu says \"%s\" and \"%s\"\n", list, appFreeSearchLine(app),
                   appFreeNextListLine(app));
            g_problems++;
        }
        app->panel = PANEL_MENU;
        app->menu_cursor = 0;
        char name[32];
        snprintf(name, sizeof(name), "free-menu-%d", list);
        render(app, dir, name);
        app->panel = PANEL_NONE;
    }
    app->free_query[0] = '\0';
    app->free_search_in = CATALOG_POPULAR;
}

// The Network tab: no folder yet; then a made-up one, its list, a copy, and its Settings page.
static void networkShots(App *app, const char *dir)
{
    // Free books, always there: X goes to it with no folder or server. One of its books was
    // downloaded before (in the card's library, without a file): marked as on the card.
    char local[512];
    catalogLocalPath(app->books_folder, "The Salt Ledger", "Mara Fennick", "#.epub", local, sizeof(local));
    ShelfBook *downloaded = shelfAdd(&app->shelf, local);
    appSwitchSource(app, 50);
    settle(app);
    int marked = 0;
    for (int i = 0; i < app->shelf.count; i++)
        marked += app->shelf.items[i].on_card ? (strcmp(app->shelf.items[i].title, "The Salt Ledger") ? 100 : 1) : 0;
    if (!downloaded || marked != 1) {
        printf("PREVIEW: Free books marks %d books as on the card, not The Salt Ledger alone\n", marked);
        g_problems++;
    }
    render(app, dir, "free");
    app->panel = PANEL_MENU;
    app->menu_cursor = 0;
    render(app, dir, "free-menu");
    app->panel = PANEL_NONE;
    app->prefs.list_view = true;
    appResort(app);
    settle(app);
    render(app, dir, "free-list");
    app->prefs.list_view = false;
    appResort(app);
    // A free book being downloaded from a catalog that doesn't say its size: what's come so far,
    // and a piece of the bar that slides, not a bar left empty.
    int fresh = 0;
    while (fresh < app->order_count && app->shelf.items[app->order[fresh]].on_card)
        fresh++;
    g_preview_copy_unknown = true;
    appFreeOpen(app, fresh, 50);
    if (!app->net_copying[0]) {
        printf("PREVIEW: opening a free book doesn't show its download\n");
        g_problems++;
    }
    render(app, dir, "free-copy-unknown-size");
    appHandleButtons(app, HidNpadButton_B, 50);
    appUpdate(app, 50);
    g_preview_copy_unknown = false;
    freeMenus(app, dir);
    // Another list whose catalog is down (Project Gutenberg's 503), reached with L/R from one with
    // books: its name over no books and the message, never the old list's books.
    appFreeShow(app, CATALOG_FRENCH, 50);
    settle(app);
    g_preview_free_down = true;
    appFreeStepList(app, 1, 50);
    bool emptied = app->order_count == 0;  // At once, while the new list is asked for.
    settle(app);
    if (!emptied || app->order_count != 0 || !app->free_problem[0] || app->free_list != CATALOG_OPEN) {
        printf("PREVIEW: a list that couldn't be read still shows %d books of the one before\n", app->order_count);
        g_problems++;
    }
    render(app, dir, "free-down");
    g_preview_free_down = false;
    appFreeShow(app, CATALOG_POPULAR, 50);
    settle(app);
    appLibTab(app, LIB_CARD, 51);
    ShelfBook *gone = shelfFind(&app->shelf, local);  // The made-up download leaves the card's library.
    if (gone)
        gone->present = false;
    appResort(app);

    g_preview_net_configured = true;
    appNetworkFeedSettings();
    appLibTab(app, LIB_NETWORK, 52);  // The network folder's library: covers read over the "network".
    settle(app);
    render(app, dir, "network");
    app->covers_busy_ns = 1234567890ull;
    render(app, dir, "network-covers-loading");
    app->covers_busy_ns = 0;
    app->panel = PANEL_MENU;
    app->menu_cursor = 0;
    render(app, dir, "network-menu");
    app->panel = PANEL_NONE;
    app->prefs.list_view = true;
    appResort(app);
    settle(app);
    render(app, dir, "network-list");
    app->prefs.list_view = false;
    appResort(app);
    appOpenBook(app, app->cursor, 54);  // Not on the card yet: copied first.
    render(app, dir, "network-copy");
    appHandleButtons(app, HidNpadButton_B, 55);
    appUpdate(app, 56);
    app->panel = PANEL_SETTINGS;
    app->settings_page = SETTINGS_PAGE_NETWORK;
    app->settings_cursor = 0;
    render(app, dir, "settings-network");
    app->panel = PANEL_NONE;
    appLibTab(app, LIB_CARD, 57);
    render(app, dir, "library-network-set");
    app->panel = PANEL_MENU;
    app->menu_cursor = 0;
    render(app, dir, "library-menu-network");
    app->panel = PANEL_NONE;

    g_preview_server_configured = true;  // A book server too.
    appServerFeedSettings();
    appServerStart(app);
    appLibTab(app, LIB_SERVER, 59);
    settle(app);
    render(app, dir, "server");
    app->panel = PANEL_MENU;
    app->menu_cursor = 0;
    render(app, dir, "server-menu");
    app->panel = PANEL_SETTINGS;
    app->settings_page = SETTINGS_PAGE_MAIN;
    app->settings_cursor = 2;
    render(app, dir, "settings-main-server");
    app->settings_page = SETTINGS_PAGE_SERVER;
    app->settings_cursor = 0;
    render(app, dir, "settings-server");
    app->panel = PANEL_NONE;
    appLibTab(app, LIB_CARD, 61);
    g_preview_server_configured = false;
    appServerFeedSettings();
    g_preview_net_configured = false;  // The rest as before: no folder.
    appNetworkFeedSettings();
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    mkdir(dir, 0755);
    if (system("rm -rf /tmp/kaireader_preview && mkdir -p /tmp/kaireader_preview/Backgrounds && python3 "
               "../make_background.py /tmp/kaireader_preview/Backgrounds/dusk.png") != 0)
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
    appOpenWelcome(&app);  // The first run's (shown with no books anywhere).
    render(&app, dir, "welcome");
    app.welcome_cursor = WELCOME_FREE;
    render(&app, dir, "welcome-free");
    appAskExit(&app);  // + twice: "Exit KaiReader?".
    render(&app, dir, "exit");
    // The looks: a picture behind the library (made by the Makefile), pink.
    app.prefs.accent = 1;
    themeApply(app.prefs.accent);
    snprintf(app.prefs.background, sizeof(app.prefs.background), "%s", "dusk.png");
    backgroundLoad(app.folder, app.prefs.background, app.prefs.background_blur);
    app.panel = PANEL_NONE;
    render(&app, dir, "looks-library");
    app.panel = PANEL_SETTINGS;
    app.settings_page = SETTINGS_PAGE_DISPLAY;
    app.settings_cursor = 1;
    render(&app, dir, "looks-display");
    app.prefs.accent = 0;
    themeApply(0);
    app.prefs.background[0] = '\0';
    backgroundLoad(app.folder, "", 0);
    app.panel = PANEL_NONE;
    app.covers_busy_ns = 1234567890ull;
    render(&app, dir, "library-covers-loading");
    app.covers_busy_ns = 0;
    networkShots(&app, dir);
    // Favorites (a star on the cover) and the list view.
    for (int i = 0; i < 2; i++) {
        int b = indexOfBook(&app, i ? "library4.epub" : "quiet.epub");
        if (b >= 0)
            app.shelf.items[app.order[b]].favorite = true;
    }
    render(&app, dir, "library-favorites-stars");
    app.prefs.list_view = true;
    appRescan(&app, 0);  // Laid out again.
    setSort(&app, SHELF_SORT_RECENT);
    render(&app, dir, "library-list");
    app.panel = PANEL_MENU;
    app.menu_cursor = 0;
    render(&app, dir, "library-list-menu");
    app.panel = PANEL_NONE;
    setSort(&app, SHELF_SORT_FAVORITES);
    render(&app, dir, "library-list-favorites");
    app.prefs.list_view = false;
    appRescan(&app, 0);
    render(&app, dir, "library-favorites");
    for (int i = 0; i < app.shelf.count; i++)
        app.shelf.items[i].favorite = false;
    setSort(&app, SHELF_SORT_FAVORITES);
    appRescan(&app, 0);
    render(&app, dir, "library-favorites-none");
    setSort(&app, SHELF_SORT_RECENT);
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

    // Bookmarks: two pages marked with the left stick, the ribbon on one, and the Bookmarks tab.
    waitMarks(&app);
    appHandleButtons(&app, HidNpadButton_StickL, 7);
    app.page += 3;
    settle(&app);
    waitMarks(&app);
    appHandleButtons(&app, HidNpadButton_StickL, 8);
    waitMarks(&app);
    render(&app, dir, "epub-bookmarked");
    app.bars = true;
    render(&app, dir, "epub-bookmarked-bars");
    app.bars = false;
    appHandleButtons(&app, HidNpadButton_X, 9);
    appHandleButtons(&app, HidNpadButton_R, 10);
    render(&app, dir, "bookmarks");
    app.panel = PANEL_NONE;

    // Two pages side by side, as Settings > Reading > Pages does it.
    app.panel = PANEL_SETTINGS;
    app.settings_page = SETTINGS_PAGE_READING;
    app.settings_cursor = rowAt(SETTINGS_PAGE_READING, SETTING_PAGES);
    appHandleButtons(&app, HidNpadButton_A, 12);
    app.panel = PANEL_NONE;
    settle(&app);
    render(&app, dir, "epub-two-pages");
    app.bars = true;
    render(&app, dir, "epub-two-pages-bars");
    app.bars = false;
    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "strip.cbz"), 13);
    settle(&app);
    render(&app, dir, "comic-two-pages");
    appHandleButtons(&app, HidNpadButton_Up, 30);  // The spread zoomed, as one.
    appHandleButtons(&app, HidNpadButton_Up, 31);
    settle(&app);
    render(&app, dir, "comic-two-pages-zoom");
    appCloseBook(&app);
    app.prefs.two_pages = false;
    // A picture in a dark page keeps its colors.
    appOpenBook(&app, indexOfBook(&app, "picture.epub"), 14);
    settle(&app);
    render(&app, dir, "picture-dark");
    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "plain.pdf"), 7);
    settle(&app);
    render(&app, dir, "pdf");
    // Zoomed in twice with up, then moved to the right with the left stick.
    appHandleButtons(&app, HidNpadButton_Up, 20);
    appHandleButtons(&app, HidNpadButton_Up, 21);
    appHandleButtons(&app, HidNpadButton_StickLRight, 22);
    appHandleButtons(&app, HidNpadButton_StickLRight, 23);
    settle(&app);
    {  // Resting, the page was rendered again at the zoom: sharp, not the fitted one scaled.
        float iw, ih, made;
        if (!appPageImageZoom(&app, app.page, &iw, &ih, &made) || made < app.zoom - 0.01f)
            printf("ZOOM: the page wasn't rendered again at %.2f (it's at %.2f)\n", app.zoom, made);
    }
    render(&app, dir, "pdf-zoom");
    app.bars = true;
    render(&app, dir, "pdf-zoom-bars");
    app.bars = false;
    appCloseBook(&app);

    // A wide page at 3x is past what a whole page can be: the part on screen comes sharp on its own.
    appOpenBook(&app, indexOfBook(&app, "wide.pdf"), 40);
    settle(&app);
    for (int i = 0; i < 5; i++)
        appHandleButtons(&app, HidNpadButton_Up, 41 + i);
    appZoomToStart(&app);  // Its top left, where the text is.
    settle(&app);
    {
        uint64_t now = 9000000000ull;
        for (int i = 0; i < 2000 && !appPagePart(&app, app.page); i++, now += 16000000ull) {
            appUpdate(&app, now);
            draw(&app, now);
            usleep(2000);
        }
    }
    if (!appPagePart(&app, app.page))
        printf("ZOOM: no sharp part of the wide page at %.2f\n", app.zoom);
    render(&app, dir, "pdf-wide-zoom");
    appCloseBook(&app);
    appOpenBook(&app, indexOfBook(&app, "strip.cbz"), 8);
    settle(&app);
    render(&app, dir, "comic");
    appCloseBook(&app);
    settle(&app);

    // Upright: as Settings > Display > Screen does it.
    app.panel = PANEL_SETTINGS;
    app.settings_page = SETTINGS_PAGE_DISPLAY;
    app.settings_cursor = rowAt(SETTINGS_PAGE_DISPLAY, SETTING_SCREEN);
    appHandleButtons(&app, HidNpadButton_A, 9);  // Screen: Upright, right side down.
    if (app.prefs.turn == GFX_TURN_NONE) {
        printf("PREVIEW: the screen didn't turn upright\n");
        g_problems++;
    }
    app.panel = PANEL_NONE;
    settle(&app);
    render(&app, dir, "upright-library");
    app.covers_busy_ns = 1234567890ull;  // Covers being read: the spinner at the top right.
    render(&app, dir, "upright-library-covers");
    app.covers_busy_ns = 0;
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

    // Other languages: the longest words (German), Spanish, and Japanese (the console's fonts).
    static const char *const LANGS[] = {"es", "de", "ja"};
    for (int i = 0; i < 3; i++) {
        char name[64];
        snprintf(app.prefs.language, sizeof(app.prefs.language), "%s", LANGS[i]);
        appUseLanguage(&app);
        snprintf(name, sizeof(name), "%s-library", LANGS[i]);
        render(&app, dir, name);
        app.panel = PANEL_MENU;
        app.menu_cursor = 0;
        snprintf(name, sizeof(name), "%s-menu", LANGS[i]);
        render(&app, dir, name);
        app.panel = PANEL_SETTINGS;
        app.settings_page = SETTINGS_PAGE_MAIN;
        app.settings_cursor = 0;
        snprintf(name, sizeof(name), "%s-settings", LANGS[i]);
        render(&app, dir, name);
        app.settings_page = SETTINGS_PAGE_DISPLAY;
        snprintf(name, sizeof(name), "%s-settings-display", LANGS[i]);
        render(&app, dir, name);
        app.settings_page = SETTINGS_PAGE_ABOUT;
        snprintf(name, sizeof(name), "%s-settings-about", LANGS[i]);
        render(&app, dir, name);
        app.settings_page = SETTINGS_PAGE_CREDITS;
        snprintf(name, sizeof(name), "%s-credits", LANGS[i]);
        render(&app, dir, name);
        appAskExit(&app);
        snprintf(name, sizeof(name), "%s-exit", LANGS[i]);
        render(&app, dir, name);
        app.panel = PANEL_NONE;
        appOpenBook(&app, indexOfBook(&app, "quiet.epub"), 12);
        settle(&app);
        app.bars = true;
        snprintf(name, sizeof(name), "%s-epub-bars", LANGS[i]);
        render(&app, dir, name);
        app.bars = false;
        appCloseBook(&app);
    }
    snprintf(app.prefs.language, sizeof(app.prefs.language), "%s", "en");
    appUseLanguage(&app);

    appFree(&app);
    readingStop();
    fontExit();
    printf("%d problems\n", g_problems);
    return g_problems ? 1 : 0;
}
