// KaiReader's state and what the buttons and the touch screen do with it.
// The screens (ui.c) only draw it. Every size comes from the layout of the
// moment: 1280 x 720, or 720 x 1280 with the console upright (gfx.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bookmarks.h"
#include "covers.h"
#include "kinetic.h"
#include "notice.h"
#include "prefs.h"
#include "reading.h"
#include "settings.h"
#include "shelf.h"
#include "sysinfo.h"
#include "touch.h"

#define APP_FOLDER_MAX 512
#define PAGE_SLOTS 8  // Two pages shown, the two after, the two before, and room.

// The library's grid, in layout units.
#define LIB_COVER_W 160
#define LIB_COVER_H 240
#define LIB_CELL_W 196
#define LIB_CELL_H 318
#define LIB_GROUP_H 46       // A group's name (an author, a genre) above its books.
#define LIB_TOP 116
#define LIB_SIDE 24          // Room at each side of the grid, at least.
#define FOOTER_H 56          // The help line at the bottom.

#define CONTENTS_ROW_H 48
#define READ_TURN_NS 180000000ull  // A page sliding to the next.

typedef enum {
    VIEW_LIBRARY,
    VIEW_READER,
} View;

typedef enum {
    PANEL_NONE,
    PANEL_MENU,      // + : a few things to do (and Settings).
    PANEL_SETTINGS,  // Settings, in pages.
    PANEL_CONTENTS,  // The book's table of contents, or its bookmarks (contents_tab).
} Panel;

typedef enum {
    MENU_CONTENTS,
    MENU_BOOKMARK,    // Bookmark the page shown, or remove its bookmark.
    MENU_LIBRARY,     // Back to the library.
    MENU_SETTINGS,
    MENU_RESCAN,
    MENU_EXIT,
    MENU_COUNT,
} MenuItem;

typedef struct {
    unsigned generation;
    int page;
    DocImage image;  // NULL pixels: free slot.
} PageSlot;

// One line of the library's grid: a group's name, or up to a row of books.
typedef struct {
    int y, h;        // Where, from the grid's top, in layout units.
    int first;       // Its first book, in order; -1 for a group's name.
    int count;       // Books on it.
    int group_book;  // A group's name: the book whose group it is.
} LibLine;

typedef struct {
    char folder[APP_FOLDER_MAX];        // KaiReader's own files (next to the NRO).
    char books_folder[APP_FOLDER_MAX];  // Where the books are.
    Shelf shelf;
    int order[SHELF_MAX_BOOKS];  // The shelf's present books in the library's order.
    int order_count;
    LibLine *lines;              // The grid, from order and the screen's width.
    int line_count;
    int columns;
    Prefs prefs;
    Covers covers;
    SysInfo sys;
    Notices notices;
    TouchTracker touch;
    bool quit;

    View view;
    Panel panel;
    int menu_cursor;
    SettingsPage settings_page;
    int settings_cursor;
    int contents_cursor;
    int contents_first;  // The first row shown.
    int contents_tab;    // 0: Contents; 1: Bookmarks.

    // Bookmarks: all books' (bookmarks.txt), and the open book's in reading order.
    Bookmarks marks;
    int book_marks[READING_MARKS_MAX];  // Indexes into marks.items.
    int book_mark_count;
    ReadingMark mark_found[READING_MARKS_MAX];  // Their pages and first words, from the reading thread;
    int mark_found_count;                       // -1 until they're found for this layout.
    unsigned mark_generation;

    // The library.
    int cursor;       // In order.
    int scroll;       // Pixels the grid is moved up.
    Kinetic kinetic;

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
    int turn_want;            // Screen: Automatic: the turn gravity asks for, and since when.
    uint64_t turn_want_ns;
    DocChapter *chapters;
    int chapter_count;
    int drag_rows;            // A drag in the contents: rows moved so far.
} App;

void appInit(App *app, const char *folder);
void appFree(App *app);  // Saves where the book open was left, and the settings.

// Each frame: takes the pages and covers made, follows the book's layout.
void appUpdate(App *app, uint64_t now_ns);
void appHandleButtons(App *app, uint64_t pressed, uint64_t now_ns);
// x and y as the panel reports them (1280 x 720): turned here with the screen.
void appHandleTouch(App *app, bool touching, int x, int y, uint64_t now_ns);

void appNotify(App *app, NoticeKind kind, const char *text, uint64_t now_ns);
// Looks for books again.
void appRescan(App *app, uint64_t now_ns);
void appOpenBook(App *app, int order_index, uint64_t now_ns);
void appCloseBook(App *app);

// The page shown first for page (the left one of two, with two pages side by side).
int appFirstShown(const App *app, int page);

// app_marks.c: bookmarks.
void appMarksLoad(App *app);
void appMarksForBook(App *app);  // The open book's, sent to the reading thread.
void appMarksUpdate(App *app);   // Each frame: their pages, once found.
bool appPageMarked(const App *app, int page);
bool appShownMarked(const App *app);  // The page shown (either of two) has a bookmark.
void appToggleMark(App *app, uint64_t now_ns);
void appMarksGo(App *app, int i);
void appMarksRemoveAt(App *app, int i, uint64_t now_ns);

// The page image of a page, if it's here.
const DocImage *appPageImage(const App *app, int page);
// The room a page gets on screen (layout units), and the layout for the reading thread.
void appPageRect(const App *app, int page, int *x, int *y, int *w, int *h);
ReadingLayout appLayout(const App *app);
// True when two pages show side by side (the setting, with the console lying down).
bool appTwoPages(const App *app);
// The column a flowing book's page is laid out in (side 1: the right page of two): where and its size.
void appColumn(const App *app, int side, int *x, int *y, int *w, int *h);
// The menu's rows now (the reader has more): into out, how many.
int appMenuRows(const App *app, MenuItem *out);
// The chapter the page is in (-1 if none), for the bars.
int appChapterOf(const App *app, int page);
// Rows the contents panel shows, for the screen's height.
int appContentsRows(void);
// The grid's left edge (the first cover's x) and the height of the library's view.
int appLibraryLeft(const App *app);
int appLibraryViewH(void);
