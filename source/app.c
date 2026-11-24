#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "approot.h"
#include "buttons.h"
#include "gfx.h"
#include "lang.h"
#include "ui_common.h"
#include "ui_hit.h"

#ifdef __SWITCH__
#define BOOKS_FOLDER "sdmc:/books"
#else
#define BOOKS_FOLDER "books"
#endif

#define SWIPE_MIN 80          // Layout pixels across for a swipe to turn the page.
#define SAVE_EVERY_NS (30ull * 1000000000ull)

static void pathIn(const App *app, const char *name, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", app->folder, name);
}

static void saveShelf(App *app)
{
    char file[APP_FOLDER_MAX + 16];
    pathIn(app, "shelf.txt", file, sizeof(file));
    shelfSave(&app->shelf, file);
}

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

static void sortShelf(App *app)
{
    app->order_count = shelfOrder(&app->shelf, app->order, SHELF_MAX_BOOKS);
    if (app->cursor >= app->order_count)
        app->cursor = app->order_count ? app->order_count - 1 : 0;
}

void appRescan(App *app, uint64_t now_ns)
{
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
    pathIn(app, "shelf.txt", path, sizeof(path));
    shelfLoad(&app->shelf, path);
    appRescan(app, 0);
    app->book = -1;
    app->turn_from = -1;
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

void appFree(App *app)
{
    rememberPlace(app);
    saveShelf(app);
    savePrefs(app);
    freeSlots(app);
    free(app->chapters);
    app->chapters = NULL;
    coversFree(&app->covers);
    shelfFree(&app->shelf);
}

ReadingLayout appLayout(const App *app)
{
    ReadingLayout l;
    l.box_w = READ_COLUMN_W;
    l.box_h = READ_COLUMN_H;
    l.fixed_w = GFX_WIDTH;
    l.fixed_h = GFX_HEIGHT;
    l.em = prefsEm(app->prefs.text_size);
    l.pixel_scale = GFX_SCALE * gfxDetail();
    l.colors = prefsColors(app->prefs.theme);
    return l;
}

void appOpenBook(App *app, int order_index, uint64_t now_ns)
{
    (void)now_ns;
    if (order_index < 0 || order_index >= app->order_count)
        return;
    appCloseBook(app);
    app->book = app->order[order_index];
    ReadingLayout layout = appLayout(app);
    const ShelfBook *b = &app->shelf.items[app->book];
    app->generation = readingOpen(b->path, &layout, b->place);
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
    sortShelf(app);
    app->cursor = 0;  // The book just read is first now.
    app->scroll = 0;
}

// A new text size or colors: the book is laid out again where it is.
static void relayout(App *app)
{
    savePrefs(app);
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

const DocImage *appPageImage(const App *app, int page)
{
    for (int i = 0; i < PAGE_SLOTS; i++) {
        const PageSlot *s = &app->slots[i];
        if (s->image.pixels && s->generation == app->generation && s->page == page)
            return &s->image;
    }
    return NULL;
}

void appPageRect(const App *app, int page, int *x, int *y, int *w, int *h)
{
    const DocImage *img = appPageImage(app, page);
    if (app->reflowable || !img) {
        *w = READ_COLUMN_W;
        *h = READ_COLUMN_H;
        *x = (GFX_WIDTH - *w) / 2;
        *y = READ_COLUMN_Y;
        return;
    }
    // A fixed page, rendered to fit the screen: its size in layout units.
    float per = GFX_SCALE * gfxDetail();
    *w = (int)(img->width / per + 0.5f);
    *h = (int)(img->height / per + 0.5f);
    *x = (GFX_WIDTH - *w) / 2;
    *y = (GFX_HEIGHT - *h) / 2;
}

int appChapterOf(const App *app, int page)
{
    int found = -1;
    for (int i = 0; i < app->chapter_count; i++)
        if (app->chapters[i].page >= 0 && app->chapters[i].page <= page)
            found = i;
    return found;
}

// Keeps a rendered page; the slot of the page farthest from the one read goes.
static void keepPage(App *app, ReadingPage *p)
{
    if (p->generation != app->generation || appPageImage(app, p->page)) {
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
    best->image = p->image;
    p->image.pixels = NULL;
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
            app->ready = false;
            readingClose();
            app->book = -1;
            app->view = VIEW_LIBRARY;
            return;
        }
        if (st.state == READING_OPEN) {
            app->ready = true;
            app->reflowable = st.reflowable;
            app->pages = st.pages;
            app->page = st.start_page;
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
    int want[READING_WANT_MAX], n = 0;
    const int around[] = {app->page, app->page + 1, app->page - 1, app->page + 2};
    for (int i = 0; i < 4 && n < READING_WANT_MAX; i++)
        if (around[i] >= 0 && around[i] < app->pages && !appPageImage(app, around[i]))
            want[n++] = around[i];
    readingWant(app->generation, want, n);
    readingSetCurrent(app->generation, app->page);
}

// Covers made, and the titles they bring: the library is sorted again by them,
// the book selected staying selected.
static void takeCovers(App *app)
{
    ReadingCover c;
    bool renamed = false;
    while (readingTakeCover(&c)) {
        ShelfBook *b = shelfFind(&app->shelf, c.path);
        if (b && !b->title[0] && c.title[0]) {
            shelfSetInfo(b, c.title, c.author);
            renamed = true;
        }
        coversPut(&app->covers, &c, b ? b->size : 0);
    }
    if (!renamed || app->order_count == 0)
        return;
    int selected = app->order[app->cursor];
    sortShelf(app);
    for (int i = 0; i < app->order_count; i++)
        if (app->order[i] == selected)
            app->cursor = i;
}

void appUpdate(App *app, uint64_t now_ns)
{
    static uint64_t last_save;
    takeCovers(app);
    if (app->view == VIEW_READER)
        followBook(app, now_ns);
    if (app->turn_from >= 0 && now_ns - app->turn_start_ns >= READ_TURN_NS)
        app->turn_from = -1;
    if (app->view == VIEW_LIBRARY) {
        int step = kineticStep(&app->kinetic, now_ns);
        if (step) {
            app->scroll += step;
            int rows = (app->order_count + LIB_COLUMNS - 1) / LIB_COLUMNS;
            int max = rows * LIB_CELL_H - (FOOTER_Y - LIB_TOP);
            max = max < 0 ? 0 : max;
            if (app->scroll < 0 || app->scroll > max) {
                app->scroll = app->scroll < 0 ? 0 : max;
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

// --- The menu ---

int appMenuRows(const App *app, MenuItem *out)
{
    int n = 0;
    bool reading = app->view == VIEW_READER;
    out[n++] = MENU_TEXT_SIZE;
    out[n++] = MENU_THEME;
    if (reading) {
        out[n++] = MENU_CONTENTS;
        out[n++] = MENU_LIBRARY;
    }
    out[n++] = MENU_CLOCK;
    out[n++] = MENU_ABOUT;
    out[n++] = MENU_EXIT;
    return n;
}

static void notifyTextSize(App *app, uint64_t now_ns)
{
    char text[64];
    snprintf(text, sizeof(text), T("Text size %d of %d"), app->prefs.text_size + 1, PREFS_TEXT_SIZES);
    appNotify(app, NOTICE_INFO, text, now_ns);
}

static void stepTextSize(App *app, int dir, uint64_t now_ns)
{
    int size = app->prefs.text_size + dir;
    if (size < 0 || size >= PREFS_TEXT_SIZES)
        return;
    app->prefs.text_size = size;
    if (app->view == VIEW_READER && app->ready && !app->reflowable) {
        savePrefs(app);
        appNotify(app, NOTICE_INFO, T("This book's pages are fixed: the text size is for books that flow."), now_ns);
        return;
    }
    notifyTextSize(app, now_ns);
    relayout(app);
}

static void stepTheme(App *app, int dir, uint64_t now_ns)
{
    app->prefs.theme = (app->prefs.theme + dir + THEME_COUNT) % THEME_COUNT;
    appNotify(app, NOTICE_INFO, T(prefsThemeName(app->prefs.theme)), now_ns);
    if (app->reflowable)
        relayout(app);
    else
        savePrefs(app);
}

static void openContents(App *app, uint64_t now_ns)
{
    if (app->view != VIEW_READER || !app->ready)
        return;
    if (app->chapter_count == 0) {
        app->panel = PANEL_NONE;
        appNotify(app, NOTICE_INFO, T("This book has no table of contents."), now_ns);
        return;
    }
    app->panel = PANEL_CONTENTS;
    int at = appChapterOf(app, app->page);
    app->contents_cursor = at < 0 ? 0 : at;
    app->contents_first = app->contents_cursor - CONTENTS_ROWS / 2;
    if (app->contents_first > app->chapter_count - CONTENTS_ROWS)
        app->contents_first = app->chapter_count - CONTENTS_ROWS;
    if (app->contents_first < 0)
        app->contents_first = 0;
}

static void menuPick(App *app, MenuItem item, int dir, uint64_t now_ns)
{
    switch (item) {
    case MENU_TEXT_SIZE:
        stepTextSize(app, dir ? dir : 1, now_ns);
        break;
    case MENU_THEME:
        stepTheme(app, dir ? dir : 1, now_ns);
        break;
    case MENU_CLOCK:
        app->prefs.clock_24h = !app->prefs.clock_24h;
        savePrefs(app);
        break;
    case MENU_CONTENTS:
        if (!dir)
            openContents(app, now_ns);
        break;
    case MENU_LIBRARY:
        if (!dir)
            appCloseBook(app);
        break;
    case MENU_ABOUT:
        if (!dir)
            app->panel = PANEL_ABOUT;
        break;
    case MENU_EXIT:
        if (!dir)
            app->quit = true;
        break;
    default:
        break;
    }
}

static bool isValueRow(MenuItem item)
{
    return item == MENU_TEXT_SIZE || item == MENU_THEME || item == MENU_CLOCK;
}

static void menuButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    MenuItem rows[MENU_COUNT];
    int n = appMenuRows(app, rows);
    if (app->menu_cursor >= n)
        app->menu_cursor = n - 1;
    MenuItem item = rows[app->menu_cursor];
    if (pressed & (HidNpadButton_B | HidNpadButton_Plus)) {
        app->panel = PANEL_NONE;
    } else if (pressed & BUTTONS_UP) {
        app->menu_cursor = (app->menu_cursor + n - 1) % n;
    } else if (pressed & BUTTONS_DOWN) {
        app->menu_cursor = (app->menu_cursor + 1) % n;
    } else if ((pressed & BUTTONS_LEFT) && isValueRow(item)) {
        menuPick(app, item, -1, now_ns);
    } else if ((pressed & BUTTONS_RIGHT) && isValueRow(item)) {
        menuPick(app, item, 1, now_ns);
    } else if (pressed & HidNpadButton_A) {
        if (!isValueRow(item))
            app->panel = PANEL_NONE;
        menuPick(app, item, 0, now_ns);
    }
}

// --- The contents ---

static void contentsGo(App *app, int entry, uint64_t now_ns)
{
    (void)now_ns;
    if (entry < 0 || entry >= app->chapter_count)
        return;
    int page = app->chapters[entry].page;
    app->panel = PANEL_NONE;
    if (page >= 0 && page < app->pages) {
        app->page = page;
        app->turn_from = -1;
    }
}

static void contentsMove(App *app, int to)
{
    if (to < 0)
        to = 0;
    if (to >= app->chapter_count)
        to = app->chapter_count - 1;
    app->contents_cursor = to;
    if (to < app->contents_first)
        app->contents_first = to;
    if (to >= app->contents_first + CONTENTS_ROWS)
        app->contents_first = to - CONTENTS_ROWS + 1;
}

static void contentsButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (pressed & (HidNpadButton_B | HidNpadButton_X))
        app->panel = PANEL_NONE;
    else if (pressed & BUTTONS_UP)
        contentsMove(app, app->contents_cursor - 1);
    else if (pressed & BUTTONS_DOWN)
        contentsMove(app, app->contents_cursor + 1);
    else if (pressed & BUTTONS_LEFT)
        contentsMove(app, app->contents_cursor - CONTENTS_ROWS);
    else if (pressed & BUTTONS_RIGHT)
        contentsMove(app, app->contents_cursor + CONTENTS_ROWS);
    else if (pressed & HidNpadButton_A)
        contentsGo(app, app->contents_cursor, now_ns);
}

// --- The library and the reader ---

static void keepCursorVisible(App *app)
{
    int row = app->cursor / LIB_COLUMNS;
    int top = row * LIB_CELL_H, bottom = top + LIB_CELL_H;
    int view = FOOTER_Y - LIB_TOP;
    if (top < app->scroll)
        app->scroll = top;
    if (bottom > app->scroll + view)
        app->scroll = bottom - view;
    kineticStop(&app->kinetic);
}

static void libraryButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    int n = app->order_count;
    if (pressed & HidNpadButton_Y) {
        appRescan(app, now_ns);
        return;
    }
    if (n == 0)
        return;
    int c = app->cursor;
    if (pressed & BUTTONS_LEFT)
        c = c > 0 ? c - 1 : c;
    else if (pressed & BUTTONS_RIGHT)
        c = c + 1 < n ? c + 1 : c;
    else if (pressed & BUTTONS_UP)
        c = c - LIB_COLUMNS >= 0 ? c - LIB_COLUMNS : c;
    else if (pressed & BUTTONS_DOWN)
        c = c + LIB_COLUMNS < n ? c + LIB_COLUMNS : (c / LIB_COLUMNS < (n - 1) / LIB_COLUMNS ? n - 1 : c);
    else if (pressed & HidNpadButton_A) {
        appOpenBook(app, c, now_ns);
        return;
    }
    if (c != app->cursor) {
        app->cursor = c;
        keepCursorVisible(app);
    }
}

static void turnPage(App *app, int dir, uint64_t now_ns)
{
    if (!app->ready)
        return;
    int to = app->page + dir;
    if (to < 0 || to >= app->pages)
        return;
    app->turn_from = appPageImage(app, app->page) && appPageImage(app, to) ? app->page : -1;
    app->turn_dir = dir;
    app->turn_start_ns = now_ns;
    app->page = to;
}

#define BUTTONS_NEXT (BUTTONS_RIGHT | HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_A)
#define BUTTONS_PREV (BUTTONS_LEFT | HidNpadButton_L | HidNpadButton_ZL)

static void readerButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (pressed & HidNpadButton_B) {
        if (app->bars)
            app->bars = false;
        else
            appCloseBook(app);
    } else if (pressed & BUTTONS_NEXT) {
        turnPage(app, 1, now_ns);
    } else if (pressed & BUTTONS_PREV) {
        turnPage(app, -1, now_ns);
    } else if (pressed & HidNpadButton_X) {
        openContents(app, now_ns);
    } else if (pressed & HidNpadButton_Y) {
        stepTheme(app, 1, now_ns);
    } else if (pressed & HidNpadButton_Minus) {
        app->bars = !app->bars;
    } else if (pressed & BUTTONS_UP) {
        stepTextSize(app, 1, now_ns);
    } else if (pressed & BUTTONS_DOWN) {
        stepTextSize(app, -1, now_ns);
    }
}

void appHandleButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (!pressed)
        return;
    if (app->panel == PANEL_ABOUT) {
        if (pressed & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus))
            app->panel = PANEL_MENU;
        return;
    }
    if (app->panel == PANEL_MENU) {
        menuButtons(app, pressed, now_ns);
        return;
    }
    if (app->panel == PANEL_CONTENTS) {
        contentsButtons(app, pressed, now_ns);
        return;
    }
    if (pressed & HidNpadButton_Plus) {
        app->panel = PANEL_MENU;
        app->menu_cursor = 0;
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
    int left = MARGIN + 140, width = GFX_WIDTH - 2 * left;
    if (app->pages <= 1 || width <= 0)
        return;
    int page = (int)((double)(x - left) / width * (app->pages - 1) + 0.5);
    page = page < 0 ? 0 : page >= app->pages ? app->pages - 1 : page;
    if (page != app->page) {
        app->page = page;
        app->turn_from = -1;
    }
}

static void tap(App *app, const HitRegion *hit, int x, uint64_t now_ns)
{
    if (!hit) {
        if (app->panel != PANEL_NONE)
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
    case HIT_BOOK:
        app->cursor = hit->arg;
        appOpenBook(app, hit->arg, now_ns);
        break;
    case HIT_OVERLAY_OUT:
        app->panel = app->panel == PANEL_ABOUT ? PANEL_MENU : PANEL_NONE;
        break;
    case HIT_MENU_ITEM: {
        MenuItem rows[MENU_COUNT];
        int n = appMenuRows(app, rows);
        if (hit->arg >= 0 && hit->arg < n) {
            app->menu_cursor = hit->arg;
            if (!isValueRow(rows[hit->arg]))
                app->panel = PANEL_NONE;
            menuPick(app, rows[hit->arg], 0, now_ns);
        }
        break;
    }
    case HIT_MENU_STEP: {
        MenuItem rows[MENU_COUNT];
        int n = appMenuRows(app, rows), row = hit->arg / 2;
        if (row >= 0 && row < n) {
            app->menu_cursor = row;
            menuPick(app, rows[row], hit->arg % 2 ? 1 : -1, now_ns);
        }
        break;
    }
    case HIT_CHAPTER:
        contentsGo(app, hit->arg, now_ns);
        break;
    case HIT_BACK:
        appCloseBook(app);
        break;
    case HIT_CONTENTS:
        openContents(app, now_ns);
        break;
    case HIT_SLIDER:
        sliderTo(app, x);
        break;
    case HIT_PAGE:
        if (x < GFX_WIDTH / 3)
            turnPage(app, -1, now_ns);
        else if (x > GFX_WIDTH * 2 / 3)
            turnPage(app, 1, now_ns);
        else
            app->bars = !app->bars;
        break;
    default:
        break;
    }
}

void appHandleTouch(App *app, bool touching, int x, int y, uint64_t now_ns)
{
    TouchEvent e = touchUpdate(&app->touch, touching, x, y, now_ns);
    switch (e.type) {
    case TOUCH_PRESS:
        kineticStop(&app->kinetic);
        break;
    case TOUCH_TAP:
        tap(app, uiHitFind(e.x, e.y), e.x, now_ns);
        break;
    case TOUCH_DRAG: {
        const HitRegion *start = uiHitFind(e.start_x, e.start_y);
        if (app->view == VIEW_LIBRARY && app->panel == PANEL_NONE) {
            app->scroll -= e.dy;
            kineticDrag(&app->kinetic, -e.dy, now_ns);
            int rows = (app->order_count + LIB_COLUMNS - 1) / LIB_COLUMNS;
            int max = rows * LIB_CELL_H - (FOOTER_Y - LIB_TOP);
            max = max < 0 ? 0 : max;
            app->scroll = app->scroll < 0 ? 0 : app->scroll > max ? max : app->scroll;
        } else if (app->view == VIEW_READER && start && start->kind == HIT_SLIDER) {
            sliderTo(app, e.x);
        } else if (app->panel == PANEL_CONTENTS && e.dy) {
            app->drag_dx += e.dy;  // Rows move by whole rows.
            while (app->drag_dx <= -48) {
                app->drag_dx += 48;
                if (app->contents_first + CONTENTS_ROWS < app->chapter_count)
                    app->contents_first++;
            }
            while (app->drag_dx >= 48) {
                app->drag_dx -= 48;
                if (app->contents_first > 0)
                    app->contents_first--;
            }
        }
        break;
    }
    case TOUCH_DRAG_END:
        if (app->view == VIEW_LIBRARY)
            kineticRelease(&app->kinetic, now_ns);
        else if (app->view == VIEW_READER && app->panel == PANEL_NONE) {
            const HitRegion *start = uiHitFind(e.start_x, e.start_y);
            int dx = e.x - e.start_x, dy = e.y - e.start_y;
            if (start && start->kind == HIT_PAGE && abs(dx) > SWIPE_MIN && abs(dx) > abs(dy))
                turnPage(app, dx < 0 ? 1 : -1, now_ns);  // Swiped left: the next page, as on paper.
        }
        app->drag_dx = 0;
        break;
    default:
        break;
    }
}
