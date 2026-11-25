#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "approot.h"
#include "buttons.h"
#include "davnet.h"
#include "languages.h"
#include "opdsnet.h"
#include "gfx.h"
#include "lang.h"
#include "motion.h"
#include "ui_common.h"
#include "ui_hit.h"

#ifdef __SWITCH__
#define BOOKS_FOLDER "sdmc:/books"
#else
#define BOOKS_FOLDER "books"
#endif

#define SWIPE_MIN 80          // Layout pixels across for a swipe to turn the page.
#define SAVE_EVERY_NS (30ull * 1000000000ull)
#define COLUMN_W_WIDE 800     // A flowing book's column, the console lying down,
#define COLUMN_SIDE_UPRIGHT 40  // its margins upright,
#define COLUMN_SIDE_TWO 40      // and with two pages: their margins and the gap between them.
#define TWO_PAGES_GAP 64
#define COLUMN_TOP 22

static void pathIn(const App *app, const char *name, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", app->folder, name);
}

static const char *const SHELF_FILES[LIB_SOURCES] = {"shelf.txt", "shelf-network.txt", "shelf-server.txt", ""};

// The library shown to its file (Free books' isn't kept: it's the catalog's page).
static void saveShelf(App *app)
{
    if (!SHELF_FILES[app->lib_tab][0])
        return;
    char file[APP_FOLDER_MAX + 32];
    pathIn(app, SHELF_FILES[app->lib_tab], file, sizeof(file));
    shelfSave(&app->shelf, file);
}

bool appSourceSet(int tab)
{
    return tab == LIB_CARD || tab == LIB_FREE || (tab == LIB_NETWORK && davConfigured()) ||
           (tab == LIB_SERVER && opdsConfigured());
}

void appSavePrefs(App *app);

static void savePrefs(App *app)
{
    char file[APP_FOLDER_MAX + 16];
    pathIn(app, "settings.txt", file, sizeof(file));
    prefsSave(&app->prefs, file);
}

void appNotify(App *app, NoticeKind kind, const char *text, uint64_t now_ns)
{
    noticePush(&app->notices, kind, text, now_ns);
}

// --- The library's grid ---

int appLibraryViewH(void)
{
    return gfxLayoutHeight() - FOOTER_H - LIB_TOP;
}

int appLibraryLeft(const App *app)
{
    if (app->prefs.list_view)
        return MARGIN + 6;
    return (gfxLayoutWidth() - app->columns * LIB_CELL_W) / 2 + (LIB_CELL_W - LIB_COVER_W) / 2;
}

static int gridHeight(const App *app)
{
    if (app->line_count == 0)
        return 0;
    const LibLine *last = &app->lines[app->line_count - 1];
    return last->y + last->h;
}

static int maxScroll(const App *app)
{
    int max = gridHeight(app) - appLibraryViewH();
    return max < 0 ? 0 : max;
}

// The grid's lines: a group's name where the group changes (by author or
// genre), then its books in rows of as many as fit across.
static void useTurnSetting(App *app);

static void layoutLibrary(App *app)
{
    int columns = (gfxLayoutWidth() - 2 * LIB_SIDE) / LIB_CELL_W;
    app->columns = app->prefs.list_view ? 1 : columns < 2 ? 2 : columns;
    int line_h = app->prefs.list_view ? LIB_ROW_H : LIB_CELL_H;
    free(app->lines);
    app->lines = malloc((size_t)(2 * app->order_count + 1) * sizeof(*app->lines));
    app->line_count = 0;
    if (!app->lines)
        return;
    int y = 0;
    char group[SHELF_TEXT_MAX], last[SHELF_TEXT_MAX] = "";
    bool grouped = app->lib_tab != LIB_FREE && (app->prefs.sort == SHELF_SORT_AUTHOR || app->prefs.sort == SHELF_SORT_GENRE);
    for (int i = 0; i < app->order_count;) {
        if (grouped) {
            shelfGroupOf(&app->shelf.items[app->order[i]], (ShelfSort)app->prefs.sort, group, sizeof(group));
            if (i == 0 || strcmp(group, last)) {
                app->lines[app->line_count++] = (LibLine){y, LIB_GROUP_H, -1, 0, i};
                y += LIB_GROUP_H;
                snprintf(last, sizeof(last), "%s", group);
            }
        }
        int n = 0;
        while (i + n < app->order_count && n < app->columns) {
            if (grouped && n > 0) {
                shelfGroupOf(&app->shelf.items[app->order[i + n]], (ShelfSort)app->prefs.sort, group, sizeof(group));
                if (strcmp(group, last))
                    break;  // The next group starts a row of its own.
            }
            n++;
        }
        app->lines[app->line_count++] = (LibLine){y, line_h, i, n, -1};
        y += line_h;
        i += n;
    }
    if (app->scroll > maxScroll(app))
        app->scroll = maxScroll(app);
}

// The line a book is on.
static int lineOf(const App *app, int book)
{
    for (int i = 0; i < app->line_count; i++) {
        const LibLine *l = &app->lines[i];
        if (l->first >= 0 && book >= l->first && book < l->first + l->count)
            return i;
    }
    return -1;
}

static void sortShelf(App *app)
{
    int selected = app->order_count && app->cursor < app->order_count ? app->order[app->cursor] : -1;
    if (app->lib_tab == LIB_FREE) {  // A catalog's list: in its own order (most popular first...).
        app->order_count = 0;
        for (int i = 0; i < app->shelf.count && app->order_count < SHELF_MAX_BOOKS; i++)
            app->order[app->order_count++] = i;
    } else {
        app->order_count = shelfOrder(&app->shelf, (ShelfSort)app->prefs.sort, app->order, SHELF_MAX_BOOKS);
    }
    app->cursor = 0;
    for (int i = 0; i < app->order_count; i++)
        if (app->order[i] == selected)
            app->cursor = i;
    layoutLibrary(app);
}

static void keepCursorVisible(App *app)
{
    int line = lineOf(app, app->cursor);
    if (line < 0)
        return;
    int top = app->lines[line].y, bottom = top + app->lines[line].h;
    if (line > 0 && app->lines[line - 1].first < 0)
        top = app->lines[line - 1].y;  // Its group's name shows too.
    if (top < app->scroll)
        app->scroll = top;
    if (bottom > app->scroll + appLibraryViewH())
        app->scroll = bottom - appLibraryViewH();
    kineticStop(&app->kinetic);
}

void appUseLanguage(App *app)
{
    char folder[APP_FOLDER_MAX + 16];
    pathIn(app, "lang", folder, sizeof(folder));
    appRootMakeFolder(folder);  // Where more languages' files go, there to be seen.
    languagesFind(folder);
    if (!languageUse(app->prefs.language))
        snprintf(app->prefs.language, sizeof(app->prefs.language), "%s", "en");
    settingsSetLanguageName(languageName(languageIndex(app->prefs.language)));
}

// Left or right on the Language row: the next language found, put in use and kept.
static void stepLanguage(App *app, int dir)
{
    int count = languagesCount();
    if (count <= 0)
        return;
    int at = (languageIndex(app->prefs.language) + (dir ? dir : 1) % count + count) % count;
    snprintf(app->prefs.language, sizeof(app->prefs.language), "%s", languageId(at));
    appUseLanguage(app);
    savePrefs(app);
}

void appRescan(App *app, uint64_t now_ns)
{
    if (app->lib_tab == LIB_NETWORK) {  // The network folder's: looked through again.
        appNetScan(app, now_ns);
        return;
    }
    if (app->lib_tab == LIB_SERVER) {  // The server's catalog: walked again.
        appServerScan(app, now_ns);
        return;
    }
    if (app->lib_tab == LIB_FREE) {  // The list: read again.
        appFreeLookAgain(app, now_ns);
        return;
    }
    int found = shelfScan(&app->shelf, app->books_folder);
    sortShelf(app);
    if (now_ns) {
        char text[96];
        snprintf(text, sizeof(text), found == 1 ? T("1 book in the library.") : T("%d books in the library."), found);
        appNotify(app, NOTICE_INFO, text, now_ns);
    }
}

void appInit(App *app, const char *folder)
{
    memset(app, 0, sizeof(*app));
    snprintf(app->folder, sizeof(app->folder), "%s", folder);
    snprintf(app->books_folder, sizeof(app->books_folder), "%s", BOOKS_FOLDER);
    appRootMakeFolder(app->books_folder);
    char path[APP_FOLDER_MAX + 16];
    pathIn(app, "covers", path, sizeof(path));
    appRootMakeFolder(path);
    coversInit(&app->covers, path);
    prefsDefaults(&app->prefs);
    pathIn(app, "settings.txt", path, sizeof(path));
    prefsLoad(&app->prefs, path);
    appUseLanguage(app);
    useTurnSetting(app);
    pathIn(app, "shelf.txt", path, sizeof(path));
    shelfLoad(&app->shelf, path);
    appRescan(app, 0);
    appMarksLoad(app);
    appZoomReset(app);
    app->book = -1;
    app->turn_from = -1;
    appNetworkStart(app);
    appNetStart(app);
    appServerStart(app);
    appNetworkUse();
    if (appWelcomeNeeded(app))  // The first run, with no books anywhere yet.
        appOpenWelcome(app);
}

const ShelfBook *appSelectedBook(const App *app)
{
    if (app->view != VIEW_LIBRARY || app->cursor < 0 || app->cursor >= app->order_count)
        return NULL;
    return &app->shelf.items[app->order[app->cursor]];
}

void appToggleFavorite(App *app, int order_index, uint64_t now_ns)
{
    if (order_index < 0 || order_index >= app->order_count)
        return;
    ShelfBook *b = &app->shelf.items[app->order[order_index]];
    b->favorite = !b->favorite;
    char name[SHELF_TEXT_MAX], text[SHELF_TEXT_MAX + 64];
    shelfDisplayName(b, name, sizeof(name));
    snprintf(text, sizeof(text), b->favorite ? T("%s: added to your favorites.") : T("%s: no longer a favorite."), name);
    appNotify(app, b->favorite ? NOTICE_DONE : NOTICE_INFO, text, now_ns);
    saveShelf(app);
    if (app->prefs.sort == SHELF_SORT_FAVORITES) {  // It leaves the list (or comes in): the cursor where it was.
        int at = order_index;
        sortShelf(app);
        app->cursor = at < app->order_count ? at : app->order_count - 1;
        app->cursor = app->cursor < 0 ? 0 : app->cursor;
        keepCursorVisible(app);
    }
}



// The screen's turn as Settings say: a fixed one, or Automatic (the motion
// sensor started, the screen as it is until the console leans).
static void useTurnSetting(App *app)
{
    if (app->prefs.turn == PREFS_TURN_AUTO) {
        motionInit();
        app->turn_want = gfxTurn();
    } else {
        motionExit();
        gfxSetTurn(app->prefs.turn);
    }
}

static void freeSlot(PageSlot *s)
{
    if (s->image.pixels)
        gfxImageChanged(s->image.pixels);
    docImageFree(&s->image);
    s->page = -1;
}

static void freeSlots(App *app)
{
    for (int i = 0; i < PAGE_SLOTS; i++)
        freeSlot(&app->slots[i]);
    appForgetParts(app);
}

static void freePart(PagePart *p)
{
    if (p->image.pixels)
        gfxImageChanged(p->image.pixels);
    docImageFree(&p->image);
    p->page = -1;
}

void appForgetParts(App *app)
{
    for (int i = 0; i < READING_PARTS_MAX; i++)
        freePart(&app->parts[i]);
    app->parts_asked_count = 0;
}

const PagePart *appPagePart(const App *app, int page)
{
    float iw, ih, made;
    if (!appPageImageZoom(app, page, &iw, &ih, &made))
        return NULL;
    for (int i = 0; i < READING_PARTS_MAX; i++) {
        const PagePart *p = &app->parts[i];
        if (p->page == page && p->image.pixels && p->zoom > made + 0.01f)
            return p;
    }
    return NULL;
}

// A part come: in the place of one for the same page, or of one not shown.
static void keepPart(App *app, ReadingPage *p)
{
    if (p->generation != app->generation || !p->image.pixels) {
        docImageFree(&p->image);
        return;
    }
    for (int i = 0; i < app->parts_asked_count; i++)  // Not waited for any more.
        if (app->parts_asked[i].page == p->page) {
            app->parts_asked[i] = app->parts_asked[--app->parts_asked_count];
            break;
        }
    PagePart *best = &app->parts[0];
    for (int i = 0; i < READING_PARTS_MAX; i++) {
        PagePart *s = &app->parts[i];
        bool shown = s->page == app->page || (appTwoPages(app) && s->page == app->page + 1);
        if (s->page == p->page || s->page < 0) {
            best = s;
            break;
        }
        if (!shown)
            best = s;
    }
    freePart(best);
    *best = (PagePart){p->page, p->zoom, p->x, p->y, p->full_w, p->full_h, p->image};
    p->image.pixels = NULL;
}

// Where the book open was left, kept in the shelf.
static void rememberPlace(App *app)
{
    if (app->book < 0 || !app->ready)
        return;
    ReadingStatus st;
    readingGetStatus(&st);
    if (st.generation != app->generation || st.state != READING_OPEN)
        return;
    ShelfBook *b = &app->shelf.items[app->book];
    shelfMarkRead(b, st.current_place, st.current_percent, (int64_t)time(NULL));
}

// The network is left for main.c to stop, after the reading thread (it may be reading a cover over it).
void appFree(App *app)
{
    rememberPlace(app);
    saveShelf(app);
    savePrefs(app);
    freeSlots(app);
    free(app->chapters);
    app->chapters = NULL;
    free(app->lines);
    app->lines = NULL;
    coversFree(&app->covers);
    for (int tab = 0; tab < LIB_SOURCES; tab++) {  // The other libraries too, to their files.
        if (tab == app->lib_tab || !appSourceSet(tab) || !SHELF_FILES[tab][0])
            continue;
        char file[APP_FOLDER_MAX + 32];
        pathIn(app, SHELF_FILES[tab], file, sizeof(file));
        shelfSave(&app->stash[tab], file);
    }
    shelfFree(&app->shelf);
    for (int tab = 0; tab < LIB_SOURCES; tab++)
        shelfFree(&app->stash[tab]);
    bookmarksFree(&app->marks);
}

// --- The reader's page ---

bool appTwoPages(const App *app)
{
    return app->prefs.two_pages && gfxTurn() == GFX_TURN_NONE;
}

void appColumn(const App *app, int side, int *x, int *y, int *w, int *h)
{
    int sw = gfxLayoutWidth(), sh = gfxLayoutHeight();
    *h = sh - COLUMN_TOP - 34;  // The page number goes under it.
    *y = COLUMN_TOP;
    if (appTwoPages(app)) {
        *w = (sw - 2 * COLUMN_SIDE_TWO - TWO_PAGES_GAP) / 2;
        *x = COLUMN_SIDE_TWO + (side ? *w + TWO_PAGES_GAP : 0);
        return;
    }
    *w = gfxTurn() ? sw - 2 * COLUMN_SIDE_UPRIGHT : COLUMN_W_WIDE;
    *x = (sw - *w) / 2;
}

// The page shown first (the left one of two): with two pages, an even one.
static int firstShown(const App *app, int page)
{
    return appTwoPages(app) ? page & ~1 : page;
}

int appFirstShown(const App *app, int page)
{
    return firstShown(app, page);
}

ReadingLayout appLayout(const App *app)
{
    int x, y, w, h;
    appColumn(app, 0, &x, &y, &w, &h);
    ReadingLayout l;
    l.box_w = (float)w;
    l.box_h = (float)h;
    l.fixed_w = (float)(appTwoPages(app) ? gfxLayoutWidth() / 2 - 12 : gfxLayoutWidth());
    l.fixed_h = (float)gfxLayoutHeight();
    l.em = prefsEm(app->prefs.text_size);
    l.pixel_scale = GFX_SCALE * gfxDetail();
    l.colors = prefsColors(app->prefs.theme);
    l.zoom = app->render_zoom > 1.0f ? app->render_zoom : 1.0f;
    return l;
}

void appOpenBook(App *app, int order_index, uint64_t now_ns)
{
    if (order_index < 0 || order_index >= app->order_count)
        return;
    if (app->lib_tab == LIB_FREE) {  // Downloaded to the card's books first, then opened as one of them.
        appFreeOpen(app, order_index, now_ns);
        return;
    }
    char local[NET_LOCAL_MAX];
    const char *open = app->shelf.items[app->order[order_index]].path;
    if (davIsLocation(open)) {  // The network folder's: its copy on the card, made first if it isn't there.
        if (!appNetReady(app, app->order[order_index], local, sizeof(local), now_ns))
            return;
        open = local;
    } else if (!strncmp(open, OPDS_PREFIX, strlen(OPDS_PREFIX))) {  // The server's: downloaded first.
        if (!appServerReady(app, app->order[order_index], local, sizeof(local), now_ns))
            return;
        open = local;
    }
    appCloseBook(app);
    app->book = app->order[order_index];
    appZoomReset(app);  // A book opens fitted.
    ReadingLayout layout = appLayout(app);
    const ShelfBook *b = &app->shelf.items[app->book];
    app->generation = readingOpen(open, &layout, b->place);
    appMarksForBook(app);
    app->ready = false;
    app->view = VIEW_READER;
    app->panel = PANEL_NONE;
    app->bars = false;
    app->page = 0;
    app->pages = 0;
    app->turn_from = -1;
    readingForgetCovers();
}

void appCloseBook(App *app)
{
    if (app->book < 0)
        return;
    rememberPlace(app);
    saveShelf(app);
    readingClose();
    freeSlots(app);
    free(app->chapters);
    app->chapters = NULL;
    app->chapter_count = 0;
    app->book = -1;
    app->ready = false;
    app->view = VIEW_LIBRARY;
    app->panel = PANEL_NONE;
    sortShelf(app);  // The book just read may move (first, sorted by recent).
    keepCursorVisible(app);
}

// A new text size, colors or screen: the book is laid out again where it is.
static void relayout(App *app)
{
    if (app->book < 0 || !app->ready)
        return;
    ReadingStatus st;
    readingGetStatus(&st);
    DocPlace place = st.generation == app->generation ? st.current_place : app->shelf.items[app->book].place;
    ReadingLayout layout = appLayout(app);
    app->generation = readingRelayout(&layout, place);
    app->ready = false;
    app->turn_from = -1;
    freeSlots(app);
}

// The slot holding page: of this layout, or (a fixed page, any_layout) one before.
static const PageSlot *slotOf(const App *app, int page, bool any_layout)
{
    const PageSlot *older = NULL;
    for (int i = 0; i < PAGE_SLOTS; i++) {
        const PageSlot *s = &app->slots[i];
        if (!s->image.pixels || s->page != page)
            continue;
        if (s->generation == app->generation)
            return s;
        older = s;
    }
    return any_layout && !app->reflowable ? older : NULL;
}

bool appPageCurrent(const App *app, int page)
{
    return slotOf(app, page, false) != NULL;
}

const DocImage *appPageImage(const App *app, int page)
{
    const PageSlot *s = slotOf(app, page, true);
    return s ? &s->image : NULL;
}

bool appPageImageZoom(const App *app, int page, float *w, float *h, float *zoom)
{
    const PageSlot *s = slotOf(app, page, true);
    if (!s)
        return false;
    *w = (float)s->image.width;
    *h = (float)s->image.height;
    *zoom = s->zoom > 0 ? s->zoom : 1.0f;
    return true;
}

void appRenderAgain(App *app)
{
    if (app->book < 0 || !app->ready)
        return;
    ReadingStatus st;
    readingGetStatus(&st);
    DocPlace place = st.generation == app->generation ? st.current_place : app->shelf.items[app->book].place;
    ReadingLayout layout = appLayout(app);
    app->generation = readingRelayout(&layout, place);  // Ready stays: a fixed book's pages are the same.
}

bool appFitRect(const App *app, int page, int *x, int *y, int *w, int *h)
{
    float iw, ih, made;
    if (app->reflowable || !appPageImageZoom(app, page, &iw, &ih, &made))
        return false;
    float per = GFX_SCALE * gfxDetail() * made;
    *w = (int)(iw / per + 0.5f);
    *h = (int)(ih / per + 0.5f);
    int middle = gfxLayoutWidth() / 2, side = appTwoPages(app) ? page & 1 : 0;
    *x = !appTwoPages(app) ? (gfxLayoutWidth() - *w) / 2 : side ? middle + 6 : middle - 6 - *w;
    *y = (gfxLayoutHeight() - *h) / 2;
    return true;
}

void appPageRect(const App *app, int page, int *x, int *y, int *w, int *h)
{
    const DocImage *img = appPageImage(app, page);
    int side = appTwoPages(app) ? page & 1 : 0;
    if (app->reflowable || !img) {
        appColumn(app, side, x, y, w, h);
        return;
    }
    // A fixed page: where it fits (two meet in the middle, as in a book), then zoomed about the
    // screen's middle and moved by the pan, both of a spread together.
    int fx, fy, fw, fh;
    appFitRect(app, page, &fx, &fy, &fw, &fh);
    float mx = gfxLayoutWidth() / 2.0f, my = gfxLayoutHeight() / 2.0f, z = app->zoom;
    *x = (int)(mx + (fx - mx) * z + app->pan_x);
    *y = (int)(my + (fy - my) * z + app->pan_y);
    *w = (int)(fw * z + 0.5f);
    *h = (int)(fh * z + 0.5f);
    (void)side;
}

int appChapterOf(const App *app, int page)
{
    int found = -1;
    for (int i = 0; i < app->chapter_count; i++)
        if (app->chapters[i].page >= 0 && app->chapters[i].page <= page)
            found = i;
    return found;
}

int appContentsRows(void)
{
    int rows = (gfxLayoutHeight() - 200) / CONTENTS_ROW_H;
    return rows < 4 ? 4 : rows;
}

// Keeps a rendered page; the slot of the page farthest from the one read goes.
static void keepPage(App *app, ReadingPage *p)
{
    if (p->part) {
        keepPart(app, p);
        return;
    }
    if (p->generation != app->generation || slotOf(app, p->page, false)) {
        docImageFree(&p->image);
        return;
    }
    PageSlot *best = NULL;
    int best_far = -1;
    for (int i = 0; i < PAGE_SLOTS; i++) {
        PageSlot *s = &app->slots[i];
        int far = !s->image.pixels || s->generation != app->generation ? 1 << 30 : abs(s->page - app->page);
        if (far > best_far) {
            best_far = far;
            best = s;
        }
    }
    freeSlot(best);
    best->generation = p->generation;
    best->page = p->page;
    best->zoom = p->zoom;
    best->image = p->image;
    p->image.pixels = NULL;
    for (int i = 0; i < PAGE_SLOTS; i++)  // The same page of a layout gone isn't needed now (zoomed, they're big).
        if (&app->slots[i] != best && app->slots[i].page == best->page && app->slots[i].generation != app->generation)
            freeSlot(&app->slots[i]);
}

static void followBook(App *app, uint64_t now_ns)
{
    ReadingStatus st;
    readingGetStatus(&st);
    if (!app->ready && st.generation == app->generation) {
        if (st.state == READING_FAILED) {
            char text[NOTICE_TEXT_MAX];
            snprintf(text, sizeof(text), T("The book couldn't be opened: %s"), st.error);
            appNotify(app, NOTICE_ERROR, text, now_ns);
            readingClose();
            app->book = -1;
            app->view = VIEW_LIBRARY;
            return;
        }
        if (st.state == READING_OPEN) {
            app->ready = true;
            app->reflowable = st.reflowable;
            app->pages = st.pages;
            app->page = firstShown(app, st.start_page);
            free(app->chapters);
            app->chapters = NULL;
            app->chapter_count = 0;
            if (st.chapters > 0) {
                app->chapters = malloc((size_t)st.chapters * sizeof(*app->chapters));
                if (app->chapters)
                    app->chapter_count = readingChapters(app->chapters, st.chapters);
            }
            ShelfBook *b = &app->shelf.items[app->book];
            if (st.title[0])
                shelfSetInfo(b, st.title, st.author);
        }
    }
    ReadingPage p;
    while (readingTakePage(&p))
        keepPage(app, &p);
    if (!app->ready)
        return;
    appMarksUpdate(app);
    int want[READING_WANT_MAX], n = 0;
    // The pages shown first, then the ones after, then before (zoomed in: only the page, they're big).
    const int one[] = {app->page, app->page + 1, app->page - 1, app->page + 2};
    const int two[] = {app->page, app->page + 1, app->page + 2, app->page + 3, app->page - 2, app->page - 1};
    const int *around = appTwoPages(app) ? two : one;
    int count = appZoomed(app) ? (appTwoPages(app) ? 2 : 1) : appTwoPages(app) ? 6 : 4;
    for (int i = 0; i < count && n < READING_WANT_MAX; i++)
        if (around[i] >= 0 && around[i] < app->pages && !slotOf(app, around[i], false))  // Not one scaled from before.
            want[n++] = around[i];
    readingWant(app->generation, want, n);
    appAskParts(app, now_ns);
    readingSetCurrent(app->generation, app->page);
}

// Covers made, and what they bring (title, author, genre): the library is
// sorted again with it, the book selected staying selected.
static void takeCovers(App *app)
{
    ReadingCover c;
    bool learned = false;
    while (readingTakeCover(&c)) {
        ShelfBook *b = shelfFind(&app->shelf, c.path);
        if (b) {
            if (!b->title[0] && c.title[0]) {
                shelfSetInfo(b, c.title, c.author);
                learned = true;
            }
            if ((c.genre[0] && strcmp(b->genre, c.genre)) ||
                (c.author_sort[0] && strcmp(b->author_sort, c.author_sort))) {
                shelfSetDetails(b, c.genre, c.author_sort);
                learned = true;
            }
        }
        coversPut(&app->covers, &c, b ? b->size : 0);
    }
    if (learned && app->view == VIEW_LIBRARY)
        sortShelf(app);
}

#define AUTO_TURN_NS 400000000ull  // How long the console must lean a new way before the screen turns.

// Screen: Automatic: the screen follows the console, once it has leaned a
// new way a moment (handheld only; detached Joy-Con are held any way).
static void autoTurn(App *app, uint64_t now_ns)
{
    float x, y, z;
    int source;
    if (app->prefs.turn != PREFS_TURN_AUTO || !motionGravity(&x, &y, &z, &source) || source != 0)
        return;
    int want = prefsTurnFromGravity(x, y, z, gfxTurn());
    if (want == gfxTurn()) {
        app->turn_want = want;
        return;
    }
    if (want != app->turn_want) {
        app->turn_want = want;
        app->turn_want_ns = now_ns;
        return;
    }
    if (now_ns - app->turn_want_ns < AUTO_TURN_NS)
        return;
    gfxSetTurn(want);
    if (want != GFX_TURN_NONE)
        app->prefs.upright = want;  // The side Vertical and the quick turn use next.
    layoutLibrary(app);
    keepCursorVisible(app);
    relayout(app);
}

void appUpdate(App *app, uint64_t now_ns)
{
    autoTurn(app, now_ns);
    appZoomTick(app, now_ns);
    static uint64_t last_save;
    takeCovers(app);
    if (readingCoversPending())
        app->covers_busy_ns = now_ns;
    appNetworkUpdate(app, now_ns);
    appNetUpdate(app, now_ns);
    appServerUpdate(app, now_ns);
    appServerTestUpdate(app, now_ns);
    appFreeUpdate(app, now_ns);
    appWelcomeTick(app);
    if (app->view == VIEW_READER)
        followBook(app, now_ns);
    if (app->turn_from >= 0 && now_ns - app->turn_start_ns >= READ_TURN_NS)
        app->turn_from = -1;
    if (app->view == VIEW_LIBRARY) {
        int step = kineticStep(&app->kinetic, now_ns);
        if (step) {
            app->scroll += step;
            if (app->scroll < 0 || app->scroll > maxScroll(app)) {
                app->scroll = app->scroll < 0 ? 0 : maxScroll(app);
                kineticStop(&app->kinetic);
            }
        }
    }
    // Where the book is, every so often (a crash or a flat battery loses little).
    if (app->book >= 0 && app->ready && now_ns - last_save > SAVE_EVERY_NS) {
        last_save = now_ns;
        rememberPlace(app);
        saveShelf(app);
    }
}

// --- Changes from Settings and the quick buttons ---

static void applyChange(App *app, int changed, uint64_t now_ns)
{
    savePrefs(app);
    if (changed & SETTINGS_CHANGED_SCREEN) {
        useTurnSetting(app);
        layoutLibrary(app);
        keepCursorVisible(app);
    }
    if (changed & SETTINGS_CHANGED_ORDER)
        sortShelf(app);
    if (changed & SETTINGS_CHANGED_LAYOUT)
        relayout(app);
    if (changed & SETTINGS_RESCAN)
        appRescan(app, now_ns);
}

static void stepSetting(App *app, SettingRow row, int dir, uint64_t now_ns)
{
    if (row == SETTING_TEXT_SIZE && app->view == VIEW_READER && app->ready && !app->reflowable) {
        appNotify(app, NOTICE_INFO, T("This book's pages are fixed: the text size is for books that flow."), now_ns);
        return;
    }
    applyChange(app, settingsStep(&app->prefs, row, dir), now_ns);
}

// The right stick's press, or the turn button on the reading bar: the screen
// upright (on the side used last) or lying down, and how to come back.
static void quickTurn(App *app, uint64_t now_ns)
{
    bool upright = prefsToggleTurn(&app->prefs, gfxTurn()) != GFX_TURN_NONE;
    applyChange(app, SETTINGS_CHANGED_SCREEN | SETTINGS_CHANGED_LAYOUT, now_ns);
    appNotify(app, NOTICE_INFO,
              upright ? T("Upright. Press the right stick (or the turn button) to lay it down again.")
                      : T("Lying down. Press the right stick (or the turn button) to stand it up."),
              now_ns);
}

// A quick button (Y, up, down) changes a setting and says to what.
static void quickStep(App *app, SettingRow row, int dir, uint64_t now_ns)
{
    if (row == SETTING_TEXT_SIZE && app->ready && !app->reflowable) {
        stepSetting(app, row, dir, now_ns);
        return;
    }
    int before = app->prefs.text_size;
    stepSetting(app, row, dir, now_ns);
    if (row == SETTING_TEXT_SIZE && app->prefs.text_size == before)
        return;  // At an end already.
    char value[64], line[160];
    bool name;
    settingsValue(&app->prefs, row, value, sizeof(value), &name);
    snprintf(line, sizeof(line), "%s: %s", T(settingsLabel(row)), name ? T(value) : value);
    appNotify(app, NOTICE_INFO, line, now_ns);
}

// --- The menu ---

int appMenuGroup(MenuItem item)
{
    switch (item) {
    case MENU_CONTENTS:
    case MENU_BOOKMARK:
    case MENU_FAVORITE:
    case MENU_FREE_SEARCH:
    case MENU_FREE_LIST:
        return 0;
    case MENU_SETTINGS:
    case MENU_EXIT:
        return 2;
    default:
        return 1;
    }
}

int appMenuRows(const App *app, MenuItem *out)
{
    int n = 0;
    if (app->view == VIEW_READER) {
        out[n++] = MENU_CONTENTS;
        out[n++] = MENU_BOOKMARK;
        out[n++] = MENU_LIBRARY;
    }
    if (app->view == VIEW_LIBRARY) {
        if (app->lib_tab == LIB_FREE) {  // Free books: its lists and search.
            out[n++] = MENU_FREE_SEARCH;
            out[n++] = MENU_FREE_LIST;
        } else if (appSelectedBook(app)) {
            out[n++] = MENU_FAVORITE;
        }
        out[n++] = MENU_SOURCE;
        if (!appSourceSet(LIB_NETWORK) && !appSourceSet(LIB_SERVER))
            out[n++] = MENU_NET_SETUP;
        out[n++] = MENU_VIEW;
        out[n++] = MENU_RESCAN;
    }
    out[n++] = MENU_SETTINGS;
    out[n++] = MENU_EXIT;
    return n;
}

// The rows of the tab shown: the contents' entries or the bookmarks.
static int listCount(const App *app)
{
    return app->contents_tab ? app->book_mark_count : app->chapter_count;
}

// Shows a tab of the contents panel, its cursor on the place read (or the first row).
static void showTab(App *app, int tab)
{
    int rows = appContentsRows();
    app->contents_tab = tab;
    int at = -1;
    if (tab == 0) {
        at = appChapterOf(app, app->page);
    } else {  // The last bookmark at or before the page shown.
        for (int i = 0; i < app->mark_found_count && i < app->book_mark_count; i++)
            if (app->mark_found[i].page <= app->page)
                at = i;
    }
    app->contents_cursor = at < 0 ? 0 : at;
    app->contents_first = app->contents_cursor - rows / 2;
    if (app->contents_first > listCount(app) - rows)
        app->contents_first = listCount(app) - rows;
    if (app->contents_first < 0)
        app->contents_first = 0;
}

static void openContents(App *app, uint64_t now_ns)
{
    if (app->view != VIEW_READER || !app->ready)
        return;
    if (app->chapter_count == 0 && app->book_mark_count == 0) {
        app->panel = PANEL_NONE;
        appNotify(app, NOTICE_INFO, T("This book has no table of contents and no bookmarks yet."), now_ns);
        return;
    }
    app->panel = PANEL_CONTENTS;
    showTab(app, app->chapter_count == 0 ? 1 : 0);
}

static void openSettings(App *app)
{
    app->panel = PANEL_SETTINGS;
    app->settings_page = SETTINGS_PAGE_MAIN;
    app->settings_cursor = 0;
}

void appSavePrefs(App *app)
{
    savePrefs(app);
}

void appOpenSettingsPage(App *app, int page)
{
    openSettings(app);
    app->settings_page = page;
}

// Settings at the network folder's page, as Back from it lands on its row of the main page.
static void openNetworkSettings(App *app)
{
    openSettings(app);
    app->settings_page = SETTINGS_PAGE_NETWORK;
}

void appResort(App *app)
{
    sortShelf(app);
    keepCursorVisible(app);
}

void appLibTab(App *app, int tab, uint64_t now_ns)
{
    if (tab < 0 || tab >= LIB_SOURCES || tab == app->lib_tab || !appSourceSet(tab))
        return;
    saveShelf(app);
    int was = app->lib_tab;  // The shown library to the stash, the one asked from it, where each was left.
    app->stash[was] = app->shelf;
    app->stash_cursor[was] = app->cursor;
    app->stash_scroll[was] = app->scroll;
    app->shelf = app->stash[tab];
    memset(&app->stash[tab], 0, sizeof(app->stash[tab]));
    app->lib_tab = tab;
    app->order_count = 0;  // The order was the other shelf's.
    sortShelf(app);
    app->cursor = app->stash_cursor[tab] < app->order_count ? app->stash_cursor[tab] : 0;
    app->scroll = app->stash_scroll[tab];
    kineticStop(&app->kinetic);
    layoutLibrary(app);
    keepCursorVisible(app);
    if (tab == LIB_NETWORK && !app->net_scanned)  // Once a session: what's in it now.
        appNetScan(app, now_ns);
    if (tab == LIB_SERVER && !app->server_scanned)
        appServerScan(app, now_ns);
    if (tab == LIB_FREE)
        appFreeEntered(app, now_ns);
}

void appOpenCardBook(App *app, const char *path, uint64_t now_ns)
{
    appLibTab(app, LIB_CARD, now_ns);
    ShelfBook *b = shelfFind(&app->shelf, path);
    if (!b)
        b = shelfAdd(&app->shelf, path);
    if (!b) {
        appNotify(app, NOTICE_ERROR, T("The library is full."), now_ns);
        return;
    }
    b->present = true;
    sortShelf(app);
    for (int i = 0; i < app->order_count; i++)
        if (!strcmp(app->shelf.items[app->order[i]].path, path)) {
            app->cursor = i;
            keepCursorVisible(app);
            appOpenBook(app, i, now_ns);
            return;
        }
}

// The next source that's set up, after tab (round to the card).
static int nextSource(int tab)
{
    for (int i = 1; i <= LIB_SOURCES; i++) {
        int next = (tab + i) % LIB_SOURCES;
        if (appSourceSet(next))
            return next;
    }
    return LIB_CARD;
}

int appNextSource(const App *app)
{
    return nextSource(app->lib_tab);
}

void appCopyStarted(App *app, int book, const char *name, const char *local)
{
    snprintf(app->net_copying, sizeof(app->net_copying), "%s", name && name[0] ? name : T("A book"));
    snprintf(app->net_copy_local, sizeof(app->net_copy_local), "%s", local);
    app->net_open = book;
    app->copy_tab = app->lib_tab;
}

void appSwitchSource(App *app, uint64_t now_ns)
{
    if (nextSource(app->lib_tab) != app->lib_tab) {  // Free books is always there: X always goes on.
        appLibTab(app, nextSource(app->lib_tab), now_ns);
        return;
    }
    openSettings(app);
    SettingRow rows[SETTINGS_ROWS_MAX];
    int n = settingsRows(SETTINGS_PAGE_MAIN, rows);
    for (int i = 0; i < n; i++)
        if (rows[i] == SETTING_OPEN_NETWORK)
            app->settings_cursor = i;
    appNotify(app, NOTICE_INFO, T("No other source yet: add a network folder in Settings."), now_ns);
}

static void menuPick(App *app, MenuItem item, uint64_t now_ns)
{
    app->panel = PANEL_NONE;
    switch (item) {
    case MENU_CONTENTS:
        openContents(app, now_ns);
        break;
    case MENU_BOOKMARK:
        appToggleMark(app, now_ns);
        break;
    case MENU_LIBRARY:
        appCloseBook(app);
        break;
    case MENU_SETTINGS:
        openSettings(app);
        break;
    case MENU_FAVORITE:
        appToggleFavorite(app, app->cursor, now_ns);
        break;
    case MENU_FREE_SEARCH:
        appFreeSearch(app, now_ns);
        break;
    case MENU_FREE_LIST:
        appFreeStepList(app, 1, now_ns);
        break;
    case MENU_VIEW:
        quickStep(app, SETTING_VIEW, 1, now_ns);
        keepCursorVisible(app);
        break;
    case MENU_SOURCE:
        appSwitchSource(app, now_ns);
        break;
    case MENU_NET_SETUP:  // Settings' main page, on the network folder (the book server under it).
        openSettings(app);
        app->settings_cursor = 1;
        break;
    case MENU_RESCAN:
        appRescan(app, now_ns);
        break;
    case MENU_EXIT:
        app->quit = true;
        break;
    default:
        break;
    }
}

static void menuButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    MenuItem rows[MENU_COUNT];
    int n = appMenuRows(app, rows);
    if (app->menu_cursor >= n)
        app->menu_cursor = n - 1;
    if (pressed & (HidNpadButton_B | HidNpadButton_Plus))
        app->panel = PANEL_NONE;
    else if (pressed & BUTTONS_UP)
        app->menu_cursor = (app->menu_cursor + n - 1) % n;
    else if (pressed & BUTTONS_DOWN)
        app->menu_cursor = (app->menu_cursor + 1) % n;
    else if (pressed & HidNpadButton_A)
        menuPick(app, rows[app->menu_cursor], now_ns);
}

// --- Settings ---

// A row of the page open: it opens its page, does its action, or steps its value.
static void settingsPick(App *app, int index, int dir, uint64_t now_ns)
{
    SettingRow rows[SETTINGS_ROWS_MAX];
    int n = settingsRows(app->settings_page, rows);
    if (index < 0 || index >= n)
        return;
    app->settings_cursor = index;
    SettingRow row = rows[index];
    if (row == SETTING_LANGUAGE) {
        stepLanguage(app, dir);
        return;
    }
    if (settingsIsServer(row)) {  // The book server's rows: the keyboard, the test, forgetting it.
        if (!dir)
            appServerActivate(app, row, now_ns);
        return;
    }
    if (settingsIsNetwork(row)) {  // The account's rows: the keyboard, or a switch saved with the account.
        bool toggle = row == SETTING_NET_REMEMBER || row == SETTING_NET_UNSIGNED;
        if (!dir || toggle)
            appNetworkActivate(app, row, now_ns);
        return;
    }
    SettingsPage opens = settingsOpens(row);
    if (opens != SETTINGS_PAGE_MAIN) {
        if (dir >= 0) {
            app->settings_page = opens;
            app->settings_cursor = 0;
        }
        return;
    }
    if (settingsHasValue(row))
        stepSetting(app, row, dir ? dir : 1, now_ns);
    else if (!dir)
        stepSetting(app, row, 0, now_ns);
}

static void settingsBack(App *app)
{
    if (app->settings_from_welcome) {  // Opened from the welcome: back to it (or on, if set up).
        app->panel = PANEL_NONE;
        app->settings_from_welcome = false;
        return;
    }
    if (app->settings_page == SETTINGS_PAGE_MAIN) {
        app->panel = PANEL_NONE;
        return;
    }
    // Back on the main page, on the row of the page left.
    SettingRow rows[SETTINGS_ROWS_MAX];
    int n = settingsRows(SETTINGS_PAGE_MAIN, rows);
    for (int i = 0; i < n; i++)
        if (settingsOpens(rows[i]) == app->settings_page)
            app->settings_cursor = i;
    app->settings_page = SETTINGS_PAGE_MAIN;
}

static void settingsButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    SettingRow rows[SETTINGS_ROWS_MAX];
    int n = settingsRows(app->settings_page, rows);
    if (pressed & HidNpadButton_Plus) {
        app->panel = PANEL_NONE;
    } else if (pressed & HidNpadButton_B) {
        settingsBack(app);
    } else if (n == 0) {  // About: only back.
        if (pressed & HidNpadButton_A)
            settingsBack(app);
    } else if (pressed & BUTTONS_UP) {
        app->settings_cursor = (app->settings_cursor + n - 1) % n;
    } else if (pressed & BUTTONS_DOWN) {
        app->settings_cursor = (app->settings_cursor + 1) % n;
    } else if (pressed & BUTTONS_LEFT) {
        settingsPick(app, app->settings_cursor, -1, now_ns);
    } else if (pressed & BUTTONS_RIGHT) {
        settingsPick(app, app->settings_cursor, 1, now_ns);
    } else if (pressed & HidNpadButton_A) {
        settingsPick(app, app->settings_cursor, 0, now_ns);
    }
}

// --- The contents ---

static void contentsGo(App *app, int entry)
{
    if (app->contents_tab) {
        appMarksGo(app, entry);
        return;
    }
    if (entry < 0 || entry >= app->chapter_count)
        return;
    int page = app->chapters[entry].page;
    app->panel = PANEL_NONE;
    if (page >= 0 && page < app->pages) {
        app->page = firstShown(app, page);
        app->turn_from = -1;
    }
}

static void contentsMove(App *app, int to)
{
    int rows = appContentsRows();
    if (to < 0)
        to = 0;
    if (to >= listCount(app))
        to = listCount(app) - 1;
    if (to < 0)
        to = 0;
    app->contents_cursor = to;
    if (to < app->contents_first)
        app->contents_first = to;
    if (to >= app->contents_first + rows)
        app->contents_first = to - rows + 1;
}

static void contentsButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    int rows = appContentsRows();
    if (pressed & (HidNpadButton_B | HidNpadButton_X))
        app->panel = PANEL_NONE;
    else if (pressed & (HidNpadButton_L | HidNpadButton_ZL | HidNpadButton_R | HidNpadButton_ZR))
        showTab(app, (pressed & (HidNpadButton_R | HidNpadButton_ZR)) ? 1 : 0);
    else if ((pressed & HidNpadButton_Y) && app->contents_tab == 1) {
        appMarksRemoveAt(app, app->contents_cursor, now_ns);
        contentsMove(app, app->contents_cursor);
    } else if (pressed & BUTTONS_UP)
        contentsMove(app, app->contents_cursor - 1);
    else if (pressed & BUTTONS_DOWN)
        contentsMove(app, app->contents_cursor + 1);
    else if (pressed & BUTTONS_LEFT)
        contentsMove(app, app->contents_cursor - rows);
    else if (pressed & BUTTONS_RIGHT)
        contentsMove(app, app->contents_cursor + rows);
    else if (pressed & HidNpadButton_A)
        contentsGo(app, app->contents_cursor);
}

// --- The library and the reader ---

static void libraryMove(App *app, int dx, int dy)
{
    int n = app->order_count, c = app->cursor;
    if (dx)
        c = c + dx < 0 ? 0 : c + dx >= n ? n - 1 : c + dx;
    if (dy) {
        int line = lineOf(app, c);
        int column = line >= 0 ? c - app->lines[line].first : 0;
        // The next line of books that way (past a group's name), at the same column or its last.
        for (int l = line + dy; l >= 0 && l < app->line_count; l += dy) {
            const LibLine *to = &app->lines[l];
            if (to->first < 0)
                continue;
            c = to->first + (column < to->count ? column : to->count - 1);
            break;
        }
    }
    if (c != app->cursor) {
        app->cursor = c;
        keepCursorVisible(app);
    }
}

static void libraryButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (app->net_copying[0]) {  // A book being copied to open: only B, to stop it.
        if (pressed & HidNpadButton_B)
            appNetCancelCopy();
        return;
    }
    if (pressed & HidNpadButton_X) {  // The source: the SD card's books, or the network folder's.
        appSwitchSource(app, now_ns);
        return;
    }
    if (app->lib_tab == LIB_FREE && (pressed & (HidNpadButton_Y | HidNpadButton_L | HidNpadButton_R))) {
        if (pressed & HidNpadButton_Y)  // Free books: Y searches, L and R change the list.
            appFreeSearch(app, now_ns);
        else
            appFreeStepList(app, (pressed & HidNpadButton_R) ? 1 : -1, now_ns);
        return;
    }
    if (pressed & HidNpadButton_Y) {
        quickStep(app, SETTING_SORT, 1, now_ns);
        keepCursorVisible(app);
        return;
    }
    if (app->order_count == 0)
        return;
    int page = appLibraryViewH() / LIB_ROW_H;
    if ((pressed & BUTTONS_LEFT) && app->prefs.list_view)
        libraryMove(app, -(page > 1 ? page : 1), 0);
    else if ((pressed & BUTTONS_RIGHT) && app->prefs.list_view)
        libraryMove(app, page > 1 ? page : 1, 0);
    else if (pressed & BUTTONS_LEFT)
        libraryMove(app, -1, 0);
    else if (pressed & BUTTONS_RIGHT)
        libraryMove(app, 1, 0);
    else if (pressed & BUTTONS_UP)
        libraryMove(app, 0, -1);
    else if (pressed & BUTTONS_DOWN)
        libraryMove(app, 0, 1);
    else if (pressed & HidNpadButton_A)
        appOpenBook(app, app->cursor, now_ns);
}

static void turnPage(App *app, int dir, uint64_t now_ns)
{
    if (!app->ready)
        return;
    int to = app->page + dir * (appTwoPages(app) ? 2 : 1);
    if (to < 0 || to >= app->pages)
        return;
    app->turn_from = appPageImage(app, app->page) && appPageImage(app, to) ? app->page : -1;
    app->turn_dir = dir;
    app->turn_start_ns = now_ns;
    app->page = to;
    appZoomToStart(app);
    if (appZoomed(app))
        app->turn_from = -1;  // A zoomed page doesn't slide: it's shown at its top left.
}

static void readerButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (appZoomed(app)) {  // The left stick moves the zoomed page; the d-pad and L/R still turn it.
        int dx = (pressed & HidNpadButton_StickLRight) ? 1 : (pressed & HidNpadButton_StickLLeft) ? -1 : 0;
        int dy = (pressed & HidNpadButton_StickLDown) ? 1 : (pressed & HidNpadButton_StickLUp) ? -1 : 0;
        if (dx || dy)
            appZoomPanStep(app, dx, dy, now_ns);
        pressed &= ~(uint64_t)(HidNpadButton_StickLLeft | HidNpadButton_StickLRight | HidNpadButton_StickLUp |
                               HidNpadButton_StickLDown);
    }
    if (app->ready && !app->reflowable && (pressed & (BUTTONS_UP | BUTTONS_DOWN))) {
        appZoomStep(app, (pressed & BUTTONS_UP) ? 1 : -1, now_ns);  // Up and down: closer or farther.
        return;
    }
    // Left and right turn the page, up and down change the text size: lying down
    // and upright alike (upright, they're turned with the console).
    uint64_t next = BUTTONS_RIGHT | HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_A;
    uint64_t prev = BUTTONS_LEFT | HidNpadButton_L | HidNpadButton_ZL;
    if (pressed & HidNpadButton_B) {
        if (app->bars)
            app->bars = false;
        else
            appCloseBook(app);
    } else if (pressed & next) {
        turnPage(app, 1, now_ns);
    } else if (pressed & prev) {
        turnPage(app, -1, now_ns);
    } else if (pressed & HidNpadButton_X) {
        openContents(app, now_ns);
    } else if (pressed & HidNpadButton_Y) {
        quickStep(app, SETTING_COLORS, 1, now_ns);
    } else if (pressed & HidNpadButton_Minus) {
        app->bars = !app->bars;
    } else if (pressed & HidNpadButton_StickL) {  // Pressed in: bookmark the page, or not.
        appToggleMark(app, now_ns);
    } else if (pressed & BUTTONS_UP) {
        quickStep(app, SETTING_TEXT_SIZE, 1, now_ns);
    } else if (pressed & BUTTONS_DOWN) {
        quickStep(app, SETTING_TEXT_SIZE, -1, now_ns);
    }
}

// The d-pad and both sticks, turned with the screen: "up" is up as the
// console is held. The right stick moves like the d-pad (KaiReader has
// nothing else for it).
static uint64_t turnDirections(uint64_t pressed)
{
    static const uint64_t UP[3] = {HidNpadButton_Up, HidNpadButton_StickLUp, HidNpadButton_StickRUp};
    static const uint64_t RIGHT[3] = {HidNpadButton_Right, HidNpadButton_StickLRight, HidNpadButton_StickRRight};
    static const uint64_t DOWN[3] = {HidNpadButton_Down, HidNpadButton_StickLDown, HidNpadButton_StickRDown};
    static const uint64_t LEFT[3] = {HidNpadButton_Left, HidNpadButton_StickLLeft, HidNpadButton_StickRLeft};
    uint64_t out = pressed;
    for (int s = 0; s < 3; s++)
        out &= ~(UP[s] | RIGHT[s] | DOWN[s] | LEFT[s]);
    for (int s = 0; s < 3; s++) {
        int d = s == 2 ? 0 : s;  // The right stick lands on the d-pad.
        bool up = pressed & UP[s], right = pressed & RIGHT[s], down = pressed & DOWN[s], left = pressed & LEFT[s];
        if (gfxTurn() == GFX_TURN_RIGHT) {  // Turned clockwise: the console's up points right.
            bool t = up;
            up = left, left = down, down = right, right = t;
        } else if (gfxTurn() == GFX_TURN_LEFT) {
            bool t = up;
            up = right, right = down, down = left, left = t;
        }
        out |= (up ? UP[d] : 0) | (right ? RIGHT[d] : 0) | (down ? DOWN[d] : 0) | (left ? LEFT[d] : 0);
    }
    return out;
}

void appHandleButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (!pressed)
        return;
    pressed = turnDirections(pressed);
    if (app->panel == PANEL_MENU) {
        menuButtons(app, pressed, now_ns);
        return;
    }
    if (app->panel == PANEL_SETTINGS) {
        settingsButtons(app, pressed, now_ns);
        return;
    }
    if (app->panel == PANEL_CONTENTS) {
        contentsButtons(app, pressed, now_ns);
        return;
    }
    if (app->panel == PANEL_WELCOME) {
        appWelcomeButtons(app, pressed, now_ns);
        return;
    }
    if (pressed & HidNpadButton_Plus) {
        app->panel = PANEL_MENU;
        app->menu_cursor = 0;
        return;
    }
    if (pressed & HidNpadButton_StickR) {  // Pressed in: stand up or lay down, in the library and reading.
        quickTurn(app, now_ns);
        return;
    }
    if (app->view == VIEW_LIBRARY)
        libraryButtons(app, pressed, now_ns);
    else
        readerButtons(app, pressed, now_ns);
}

// --- Touch ---

static void sliderTo(App *app, int x)
{
    int left = UI_SLIDER_LEFT, width = gfxLayoutWidth() - 2 * left;
    if (app->pages <= 1 || width <= 0)
        return;
    int page = (int)((double)(x - left) / width * (app->pages - 1) + 0.5);
    page = firstShown(app, page < 0 ? 0 : page >= app->pages ? app->pages - 1 : page);
    if (page != app->page) {
        app->page = page;
        app->turn_from = -1;
    }
}

static void tap(App *app, const HitRegion *hit, int x, uint64_t now_ns)
{
    if (!hit) {
        app->panel = PANEL_NONE;
        return;
    }
    switch (hit->kind) {
    case HIT_NOTICE:
        noticeDismiss(&app->notices, (uint64_t)hit->arg, now_ns);
        break;
    case HIT_EXIT:
        app->quit = true;
        break;
    case HIT_MENU_BUTTON:
        app->panel = PANEL_MENU;
        app->menu_cursor = 0;
        break;
    case HIT_RESCAN:
        appRescan(app, now_ns);
        break;
    case HIT_SOURCE:
        appSwitchSource(app, now_ns);
        break;
    case HIT_NET_SETUP:  // No folder yet: its Settings page.
        openNetworkSettings(app);
        break;
    case HIT_RESCAN_NET:  // Look again: the folder, or the server's catalog.
        appRescan(app, now_ns);
        break;
    case HIT_NET_CANCEL:
        appNetCancelCopy();
        break;
    case HIT_SORT:  // The sort; in Free books, the search.
        if (app->lib_tab == LIB_FREE)
            appFreeSearch(app, now_ns);
        else
            quickStep(app, SETTING_SORT, 1, now_ns);
        break;
    case HIT_BOOK:
        app->cursor = hit->arg;
        appOpenBook(app, hit->arg, now_ns);
        break;
    case HIT_OVERLAY_OUT:  // Outside a panel: it closes (the welcome waits for an answer).
        if (app->panel != PANEL_WELCOME)
            app->panel = PANEL_NONE;
        break;
    case HIT_WELCOME_ROW:
        appWelcomeChoose(app, hit->arg, now_ns);
        break;
    case HIT_MENU_ITEM: {
        MenuItem rows[MENU_COUNT];
        int n = appMenuRows(app, rows);
        if (hit->arg >= 0 && hit->arg < n) {
            app->menu_cursor = hit->arg;
            menuPick(app, rows[hit->arg], now_ns);
        }
        break;
    }
    case HIT_SETTING:
        settingsPick(app, hit->arg, 0, now_ns);
        break;
    case HIT_SETTING_STEP:
        settingsPick(app, hit->arg / 2, hit->arg % 2 ? 1 : -1, now_ns);
        break;
    case HIT_SETTINGS_BACK:
        settingsBack(app);
        break;
    case HIT_CHAPTER:
        contentsGo(app, hit->arg);
        break;
    case HIT_BACK:
        appCloseBook(app);
        break;
    case HIT_CONTENTS:
        openContents(app, now_ns);
        break;
    case HIT_TURN:
        quickTurn(app, now_ns);
        break;
    case HIT_MARK_TOGGLE:
        appToggleMark(app, now_ns);
        break;
    case HIT_TAB:
        showTab(app, hit->arg);
        break;
    case HIT_SLIDER:
        sliderTo(app, x);
        break;
    case HIT_PAGE: {
        int w = gfxLayoutWidth();
        if (x < w / 3)
            turnPage(app, -1, now_ns);
        else if (x > w * 2 / 3)
            turnPage(app, 1, now_ns);
        else
            app->bars = !app->bars;
        break;
    }
    default:  // Panels' backgrounds, the grid: nothing.
        break;
    }
}

// The panel's 1280 x 720 point, as the screen is turned.
static void turnPoint(int *x, int *y)
{
    int px = *x, py = *y;
    if (gfxTurn() == GFX_TURN_RIGHT) {
        *x = GFX_HEIGHT - py;
        *y = px;
    } else if (gfxTurn() == GFX_TURN_LEFT) {
        *x = py;
        *y = GFX_WIDTH - px;
    }
}

void appTurnPoint(int *x, int *y)
{
    turnPoint(x, y);
}

void appHandleTouch(App *app, bool touching, int x, int y, uint64_t now_ns)
{
    turnPoint(&x, &y);
    TouchEvent e = touchUpdate(&app->touch, touching, x, y, now_ns);
    switch (e.type) {
    case TOUCH_PRESS:
        kineticStop(&app->kinetic);
        break;
    case TOUCH_TAP:
        tap(app, uiHitFind(e.x, e.y), e.x, now_ns);
        break;
    case TOUCH_LONG: {  // Held on a cover: a favorite, or not any more.
        const HitRegion *hit = uiHitFind(e.x, e.y);
        if (hit && hit->kind == HIT_BOOK && app->panel == PANEL_NONE && app->lib_tab != LIB_FREE) {
            app->cursor = hit->arg;
            appToggleFavorite(app, hit->arg, now_ns);
        }
        break;
    }
    case TOUCH_DRAG: {
        const HitRegion *start = uiHitFind(e.start_x, e.start_y);
        if (app->view == VIEW_LIBRARY && app->panel == PANEL_NONE) {
            app->scroll -= e.dy;
            kineticDrag(&app->kinetic, -e.dy, now_ns);
            app->scroll = app->scroll < 0 ? 0 : app->scroll > maxScroll(app) ? maxScroll(app) : app->scroll;
        } else if (app->view == VIEW_READER && app->panel == PANEL_NONE && start && start->kind == HIT_SLIDER) {
            sliderTo(app, e.x);
        } else if (app->view == VIEW_READER && app->panel == PANEL_NONE && appZoomed(app) && start &&
                   start->kind == HIT_PAGE) {
            appZoomPan(app, (float)e.dx, (float)e.dy, now_ns);  // The zoomed page follows the finger.
        } else if (app->panel == PANEL_CONTENTS) {
            // Rows follow the finger, a whole row at a time.
            int rows = (e.start_y - e.y) / CONTENTS_ROW_H;
            int moved = rows - app->drag_rows;
            app->drag_rows = rows;
            int max = listCount(app) - appContentsRows();
            max = max < 0 ? 0 : max;
            int first = app->contents_first + moved;
            app->contents_first = first < 0 ? 0 : first > max ? max : first;
        }
        break;
    }
    case TOUCH_DRAG_END:
        if (app->view == VIEW_LIBRARY && app->panel == PANEL_NONE) {
            kineticRelease(&app->kinetic, now_ns);
        } else if (app->view == VIEW_READER && app->panel == PANEL_NONE) {
            const HitRegion *start = uiHitFind(e.start_x, e.start_y);
            int dx = e.x - e.start_x, dy = e.y - e.start_y;
            if (start && start->kind == HIT_PAGE && !appZoomed(app) && abs(dx) > SWIPE_MIN && abs(dx) > abs(dy))
                turnPage(app, dx < 0 ? 1 : -1, now_ns);  // Swiped left: the next page, as on paper.
        }
        app->drag_rows = 0;
        break;
    default:
        break;
    }
}
