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
#define COLUMN_W_WIDE 800     // A flowing book's column, the console lying down,
#define COLUMN_SIDE_UPRIGHT 40  // and its margins upright.
#define COLUMN_TOP 22

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

// --- The library's grid ---

int appLibraryViewH(void)
{
    return gfxLayoutHeight() - FOOTER_H - LIB_TOP;
}

int appLibraryLeft(const App *app)
{
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
static void layoutLibrary(App *app)
{
    int columns = (gfxLayoutWidth() - 2 * LIB_SIDE) / LIB_CELL_W;
    app->columns = columns < 2 ? 2 : columns;
    free(app->lines);
    app->lines = malloc((size_t)(2 * app->order_count + 1) * sizeof(*app->lines));
    app->line_count = 0;
    if (!app->lines)
        return;
    int y = 0;
    char group[SHELF_TEXT_MAX], last[SHELF_TEXT_MAX] = "";
    bool grouped = app->prefs.sort == SHELF_SORT_AUTHOR || app->prefs.sort == SHELF_SORT_GENRE;
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
        app->lines[app->line_count++] = (LibLine){y, LIB_CELL_H, i, n, -1};
        y += LIB_CELL_H;
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
    app->order_count = shelfOrder(&app->shelf, (ShelfSort)app->prefs.sort, app->order, SHELF_MAX_BOOKS);
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
    gfxSetTurn(app->prefs.turn);
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
    free(app->lines);
    app->lines = NULL;
    coversFree(&app->covers);
    shelfFree(&app->shelf);
}

// --- The reader's page ---

void appColumn(int *x, int *y, int *w, int *h)
{
    int sw = gfxLayoutWidth(), sh = gfxLayoutHeight();
    *w = gfxTurn() ? sw - 2 * COLUMN_SIDE_UPRIGHT : COLUMN_W_WIDE;
    *h = sh - COLUMN_TOP - 34;  // The page number goes under it.
    *x = (sw - *w) / 2;
    *y = COLUMN_TOP;
}

ReadingLayout appLayout(const App *app)
{
    int x, y, w, h;
    appColumn(&x, &y, &w, &h);
    ReadingLayout l;
    l.box_w = (float)w;
    l.box_h = (float)h;
    l.fixed_w = (float)gfxLayoutWidth();
    l.fixed_h = (float)gfxLayoutHeight();
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
        appColumn(x, y, w, h);
        return;
    }
    // A fixed page, rendered to fit the screen: its size in layout units.
    float per = GFX_SCALE * gfxDetail();
    *w = (int)(img->width / per + 0.5f);
    *h = (int)(img->height / per + 0.5f);
    *x = (gfxLayoutWidth() - *w) / 2;
    *y = (gfxLayoutHeight() - *h) / 2;
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
        gfxSetTurn(app->prefs.turn);
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

int appMenuRows(const App *app, MenuItem *out)
{
    int n = 0;
    if (app->view == VIEW_READER) {
        out[n++] = MENU_CONTENTS;
        out[n++] = MENU_LIBRARY;
    }
    out[n++] = MENU_SETTINGS;
    if (app->view == VIEW_LIBRARY)
        out[n++] = MENU_RESCAN;
    out[n++] = MENU_EXIT;
    return n;
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
    int rows = appContentsRows();
    app->panel = PANEL_CONTENTS;
    int at = appChapterOf(app, app->page);
    app->contents_cursor = at < 0 ? 0 : at;
    app->contents_first = app->contents_cursor - rows / 2;
    if (app->contents_first > app->chapter_count - rows)
        app->contents_first = app->chapter_count - rows;
    if (app->contents_first < 0)
        app->contents_first = 0;
}

static void openSettings(App *app)
{
    app->panel = PANEL_SETTINGS;
    app->settings_page = SETTINGS_PAGE_MAIN;
    app->settings_cursor = 0;
}

static void menuPick(App *app, MenuItem item, uint64_t now_ns)
{
    app->panel = PANEL_NONE;
    switch (item) {
    case MENU_CONTENTS:
        openContents(app, now_ns);
        break;
    case MENU_LIBRARY:
        appCloseBook(app);
        break;
    case MENU_SETTINGS:
        openSettings(app);
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
    int rows = appContentsRows();
    if (to < 0)
        to = 0;
    if (to >= app->chapter_count)
        to = app->chapter_count - 1;
    app->contents_cursor = to;
    if (to < app->contents_first)
        app->contents_first = to;
    if (to >= app->contents_first + rows)
        app->contents_first = to - rows + 1;
}

static void contentsButtons(App *app, uint64_t pressed)
{
    int rows = appContentsRows();
    if (pressed & (HidNpadButton_B | HidNpadButton_X))
        app->panel = PANEL_NONE;
    else if (pressed & BUTTONS_UP)
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
    if (pressed & HidNpadButton_Y) {
        quickStep(app, SETTING_SORT, 1, now_ns);
        keepCursorVisible(app);
        return;
    }
    if (app->order_count == 0)
        return;
    if (pressed & BUTTONS_LEFT)
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
    int to = app->page + dir;
    if (to < 0 || to >= app->pages)
        return;
    app->turn_from = appPageImage(app, app->page) && appPageImage(app, to) ? app->page : -1;
    app->turn_dir = dir;
    app->turn_start_ns = now_ns;
    app->page = to;
}

static void readerButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    // Upright, every direction turns the page (down and right forward); lying
    // down, up and down change the text size.
    bool upright = gfxTurn() != GFX_TURN_NONE;
    uint64_t next = BUTTONS_RIGHT | HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_A;
    uint64_t prev = BUTTONS_LEFT | HidNpadButton_L | HidNpadButton_ZL;
    if (upright) {
        next |= BUTTONS_DOWN;
        prev |= BUTTONS_UP;
    }
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
        contentsButtons(app, pressed);
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
    int left = UI_SLIDER_LEFT, width = gfxLayoutWidth() - 2 * left;
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
    case HIT_SORT:
        quickStep(app, SETTING_SORT, 1, now_ns);
        break;
    case HIT_BOOK:
        app->cursor = hit->arg;
        appOpenBook(app, hit->arg, now_ns);
        break;
    case HIT_OVERLAY_OUT:
        app->panel = PANEL_NONE;
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
    case TOUCH_DRAG: {
        const HitRegion *start = uiHitFind(e.start_x, e.start_y);
        if (app->view == VIEW_LIBRARY && app->panel == PANEL_NONE) {
            app->scroll -= e.dy;
            kineticDrag(&app->kinetic, -e.dy, now_ns);
            app->scroll = app->scroll < 0 ? 0 : app->scroll > maxScroll(app) ? maxScroll(app) : app->scroll;
        } else if (app->view == VIEW_READER && app->panel == PANEL_NONE && start && start->kind == HIT_SLIDER) {
            sliderTo(app, e.x);
        } else if (app->panel == PANEL_CONTENTS) {
            // Rows follow the finger, a whole row at a time.
            int rows = (e.start_y - e.y) / CONTENTS_ROW_H;
            int moved = rows - app->drag_rows;
            app->drag_rows = rows;
            int max = app->chapter_count - appContentsRows();
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
            if (start && start->kind == HIT_PAGE && abs(dx) > SWIPE_MIN && abs(dx) > abs(dy))
                turnPage(app, dx < 0 ? 1 : -1, now_ns);  // Swiped left: the next page, as on paper.
        }
        app->drag_rows = 0;
        break;
    default:
        break;
    }
}
