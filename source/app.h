// KaiReader's state and what the buttons and the touch screen do with it.
// The screens (ui.c) only draw it.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "covers.h"
#include "kinetic.h"
#include "notice.h"
#include "prefs.h"
#include "reading.h"
#include "shelf.h"
#include "sysinfo.h"
#include "touch.h"

#define APP_FOLDER_MAX 512
#define PAGE_SLOTS 5

// The library's grid, in layout units.
#define LIB_COLUMNS 6
#define LIB_COVER_W 160
#define LIB_COVER_H 240
#define LIB_CELL_W 196
#define LIB_CELL_H 318
#define LIB_TOP 116
#define LIB_LEFT ((1280 - LIB_COLUMNS * LIB_CELL_W) / 2 + (LIB_CELL_W - LIB_COVER_W) / 2)

// The reader's page: a column for books that flow, the whole screen for fixed pages.
#define READ_COLUMN_W 800
#define READ_COLUMN_H 664
#define READ_COLUMN_Y 22
#define CONTENTS_ROWS 11  // Rows the contents panel shows.
#define READ_TURN_NS 180000000ull  // A page sliding to the next.

typedef enum {
    VIEW_LIBRARY,
    VIEW_READER,
} View;

typedef enum {
    PANEL_NONE,
    PANEL_MENU,      // + : text size, colors, contents, library, about, exit.
    PANEL_CONTENTS,  // The book's table of contents.
    PANEL_ABOUT,
} Panel;

typedef enum {
    MENU_TEXT_SIZE,
    MENU_THEME,
    MENU_CONTENTS,
    MENU_LIBRARY,
    MENU_CLOCK,
    MENU_ABOUT,
    MENU_EXIT,
    MENU_COUNT,
} MenuItem;

typedef struct {
    unsigned generation;
    int page;
    DocImage image;  // NULL pixels: free slot.
} PageSlot;

typedef struct {
    char folder[APP_FOLDER_MAX];        // KaiReader's own files (next to the NRO).
    char books_folder[APP_FOLDER_MAX];  // Where the books are.
    Shelf shelf;
    int order[SHELF_MAX_BOOKS];  // The shelf's present books in the library's order.
    int order_count;
    Prefs prefs;
    Covers covers;
    SysInfo sys;
    Notices notices;
    TouchTracker touch;
    bool quit;

    View view;
    Panel panel;
    int menu_cursor;
    int contents_cursor;
    int contents_first;  // The first row shown.

    // The library.
    int cursor;       // In order.
    int scroll;       // Pixels the grid is moved up.
    Kinetic kinetic;
    bool dragging;

    // The book open.
    int book;                 // Its index in the shelf; -1 when none.
    unsigned generation;      // Of the layout asked for.
    bool ready;               // That generation came.
    bool reflowable;
    int page, pages;
    PageSlot slots[PAGE_SLOTS];
    bool bars;                // The bars over the page (title, slider) are shown.
    int turn_from;            // A page turn's animation: the page before, or -1.
    int turn_dir;             // +1 forward, -1 back.
    uint64_t turn_start_ns;
    DocChapter *chapters;
    int chapter_count;
    bool slider_drag;
    int drag_dx;              // A drag across the page: how far, for a swipe.
} App;

void appInit(App *app, const char *folder);
void appFree(App *app);  // Saves where the book open was left, and the settings.

// Each frame: takes the pages and covers made, follows the book's layout.
void appUpdate(App *app, uint64_t now_ns);
void appHandleButtons(App *app, uint64_t pressed, uint64_t now_ns);
void appHandleTouch(App *app, bool touching, int x, int y, uint64_t now_ns);

void appNotify(App *app, NoticeKind kind, const char *text, uint64_t now_ns);
// Looks for books again.
void appRescan(App *app, uint64_t now_ns);
void appOpenBook(App *app, int order_index, uint64_t now_ns);
void appCloseBook(App *app);

// The page image of a page, if it's here.
const DocImage *appPageImage(const App *app, int page);
// The room a page gets on screen (layout units), and the layout for the reading thread.
void appPageRect(const App *app, int page, int *x, int *y, int *w, int *h);
ReadingLayout appLayout(const App *app);
// The menu's rows now (the reader has more): into out, how many.
int appMenuRows(const App *app, MenuItem *out);
// The chapter the page is in (-1 if none), for the bars.
int appChapterOf(const App *app, int page);
