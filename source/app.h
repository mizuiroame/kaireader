// KaiReader's state and what the buttons and the touch screen do with it.
// The screens (ui.c) only draw it. Every size comes from the layout of the
// moment: 1280 x 720, or 720 x 1280 with the console upright (gfx.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bookmarks.h"
#include "covers.h"
#include "dav.h"
#include "feedlist.h"
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
#define LIB_ROW_H 104        // A book's row in the list view, and its small cover.
#define LIB_ROW_COVER_W 60
#define LIB_ROW_COVER_H 90
#define LIB_TOP 116
#define LIB_SIDE 24          // Room at each side of the grid, at least.
#define FOOTER_H 56          // The help line at the bottom.
// The news' showcase (app_news.c, ui_news.c): a lead story across the width, then rows of wide cards.
#define NEWS_GAP 24          // Between cards.
#define NEWS_CARD_TEXT 108   // Under a card's picture: its headline (two lines) and feed and time.
#define NEWS_LEAD_SHARE 46   // Percent of the width the lead story's picture takes (the console lying down).
#define NET_LOCAL_MAX (APP_FOLDER_MAX + 512)
#define LIB_CARD 0     // The library's sources: the SD card's books,
#define LIB_NETWORK 1  // the network folder's,
#define LIB_SERVER 2   // the book server's (OPDS),
#define LIB_FREE 3     // the free catalogs' (app_free.c),
#define LIB_NEWS 4     // and the news feeds' articles (app_news.c).
#define LIB_SOURCES 5

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
    PANEL_WELCOME,   // The first run's: where are the books? (app_welcome.c)
    PANEL_EXIT,      // "Exit KaiReader?" (+ twice, or the X), as KaiRadio's.
} Panel;

typedef enum {
    WELCOME_CARD,
    WELCOME_NETWORK,
    WELCOME_SERVER,
    WELCOME_FREE,
    WELCOME_NEWS,
    WELCOME_LATER,
    WELCOME_ROW_COUNT,
} WelcomeRow;

typedef enum {
    MENU_CONTENTS,
    MENU_BOOKMARK,    // Bookmark the page shown, or remove its bookmark.
    MENU_LIBRARY,     // Back to the library.
    MENU_FAVORITE,    // The book selected in the library: a favorite, or not any more.
    MENU_VIEW,        // The library as a list, or as a grid.
    MENU_SOURCE,      // The library: the SD card's books, or the network folder's.
    MENU_NET_SETUP,   // No other source yet: Settings, at the network folder and the book server.
    MENU_SETTINGS,
    MENU_RESCAN,      // The card's books looked for again, or the network folder read again.
    MENU_FREE_SEARCH, // Free books: search the shown list's catalog.
    MENU_FREE_LIST,   // Free books: the next list.
    MENU_NEWS_READ,   // News: every article shown marked as read.
    MENU_NEWS_FEED,   // News: the next feed (or all of them).
    MENU_NEWS_FULL,   // Reading an article: the whole of it, from its site.
    MENU_EXIT,
    MENU_COUNT,
} MenuItem;

typedef struct {
    unsigned generation;
    int page;
    float zoom;      // The zoom it was rendered at (a fixed page; 1 for fitted).
    DocImage image;  // NULL pixels: free slot.
} PageSlot;

// The part of a zoomed page on screen, rendered sharp on its own (the whole page
// can't be that big): from x, y of the page at its zoom, full_w x full_h.
typedef struct {
    int page;  // -1: free.
    float zoom;
    int x, y, full_w, full_h;
    DocImage image;
} PagePart;

// One line of the library's grid: a group's name, or up to a row of books.
typedef struct {
    int y, h;        // Where, from the grid's top, in layout units.
    int first;       // Its first book, in order; -1 for a group's name.
    int count;       // Books on it.
    int group_book;  // A group's name: the book whose group it is.
    bool lead;       // The news' showcase: its first story, large, across the whole width.
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

    // The library: its tabs, the books on the card (0) and the network folder (1, app_netlib.c).
    // The library's source (LIB_*): shelf is its books; the others' wait in stash (X swaps them),
    // with where each was left.
    int lib_tab;
    Shelf stash[LIB_SOURCES];
    int stash_cursor[LIB_SOURCES], stash_scroll[LIB_SOURCES];
    bool net_scanned;                    // The network folder looked through since KaiReader started.
    bool server_scanned;                 // The book server's catalog walked since KaiReader started.
    char server_problem[256];            // Why the server couldn't be read; "" if it could.
    int copy_tab;                        // The library of the book being copied to open.
    bool resume_pending;                 // The last book to open again, on the first frame.
    unsigned update_seen;                // The update status last said.
    bool update_manual;                  // Asked from Settings: "up to date" and failures are said.
    int welcome_cursor;
    bool welcome_away;                   // Gone to a Settings page from the welcome: back to it if unset.
    bool settings_from_welcome;          // B on that page goes back to the welcome.
    int free_list;                       // Free books: the list shown (catalogs.h's CatalogList),
    char free_query[128];                // the last search,
    int free_search_in;                  // the list whose catalog it searched (a CatalogList),
    char free_asked[1024];               // the page asked for last, and whether it adds to the list;
    bool free_more, free_loading;
    char free_next[1024];                // the list's next page ("" at its end),
    char free_problem[256];              // and why the list couldn't be read.
    FeedList feeds;                      // News: the feeds followed (feeds.txt),
    int news_settings_feed;              // the one Settings > News's rows are about,
    int news_suggest_lang;               // the suggested feeds' language (feedsuggest.h),
    bool news_checked;                   // the feeds checked since KaiReader started,
    char news_problem[256];              // why none could be read ("" if one could),
    char news_full[SHELF_PATH_MAX];      // the article whose whole page is being fetched ("" none),
    char news_full_link[FEED_URL_MAX];   // from there,
    char news_pictures[SHELF_PATH_MAX];  // and the one whose pictures are ("" none).
    char net_problem[256];               // Why the folder couldn't be read; "" if it could.
    int net_open;                        // A network book copied to be opened: its shelf index; -1 none.
    char net_copying[DAV_NAME_MAX];      // The book being copied to the card; "" none.
    char net_copy_local[NET_LOCAL_MAX];  // Where it goes.

    // The books on the card.
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
    PagePart parts[READING_PARTS_MAX];
    ReadingPartAsk parts_asked[READING_PARTS_MAX];  // Asked and not come yet, of parts_generation.
    int parts_asked_count;
    unsigned parts_generation;
    uint64_t view_moved_ns;  // The zoomed view last moved (zoom or pan).
    uint64_t covers_busy_ns; // Covers were last being made then (the spinner stays a moment after).
    bool bars;                // The bars over the page (title, slider) are shown.
    int turn_from;            // A page turn's animation: the page before, or -1.
    int turn_dir;             // +1 forward, -1 back.
    uint64_t turn_start_ns;
    int turn_want;            // Screen: Automatic: the turn gravity asks for, and since when.
    uint64_t turn_want_ns;
    // Zoom on a fixed page (app_zoom.c): shown, and rendered at; the page middle's offset.
    float zoom, render_zoom;
    float pan_x, pan_y;
    uint64_t zoom_changed_ns;
    bool pinching;            // Two fingers on the screen: zooming.
    bool touch_held;          // After two fingers, until every finger is up: no tap, no drag.
    float pinch_distance, pinch_zoom, pinch_x, pinch_y;
    DocChapter *chapters;
    int chapter_count;
    // A news article reads as a web page (app_scroll.c): its strips under one another, scrolled.
    bool scroll_view;
    float scroll_y;           // Layout pixels the article is moved up.
    int scroll_end;           // Where the last strip's text ends (layout pixels); -1 not known yet.
    float scroll_restore;     // Opened again: where it was scrolled to; -1 none.
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
// The first run's welcome (app_welcome.c).
bool appWelcomeNeeded(const App *app);
void appOpenWelcome(App *app);
void appWelcomeChoose(App *app, int row, uint64_t now_ns);
void appWelcomeButtons(App *app, uint64_t pressed, uint64_t now_ns);
void appWelcomeTick(App *app);
// Settings open at a page (B there closes it); and the settings written to the card.
void appOpenSettingsPage(App *app, int page);
void appSavePrefs(App *app);
// "Exit KaiReader?": Exit or Cancel (+ there exits).
void appAskExit(App *app);
// Settings > About's updates (app_update.c): started with argv[0], stopped before the network.
void appStartUpdates(App *app, const char *nro_path);
void appStopUpdates(void);
void appUpdatesTick(App *app, uint64_t now_ns);
void appUpdatesActivate(App *app, uint64_t now_ns);
void appUpdatesValue(const App *app, int row, char *out, size_t size);
// The language in Settings put in use (at start, and when it changes).
void appUseLanguage(App *app);
// The book at a place in the library's order: a favorite, or not any more (with a notice).
void appToggleFavorite(App *app, int order_index, uint64_t now_ns);
// The book selected in the library, if there's one (the card's books shown).
const ShelfBook *appSelectedBook(const App *app);

// The network folder's Settings rows and test (app_network.c).
void appNetworkStart(App *app);  // Its account read from the card, the network started if there's one.
void appNetworkExit(void);
bool appNetworkActivate(App *app, SettingRow row, uint64_t now_ns);
void appNetworkUpdate(App *app, uint64_t now_ns);
void appNetworkFeedSettings(void);
// A password not remembered is asked for (the keyboard); false if there's none.
bool appNetworkEnsurePassword(App *app, uint64_t now_ns);

// The library's source: the card's books (0) or the network folder's (1), each its own shelf.
void appLibTab(App *app, int tab, uint64_t now_ns);
// X in the library: the other source, or (none yet) Settings with how to add a network folder.
void appSwitchSource(App *app, uint64_t now_ns);
// The source X goes to next (round to the card).
int appNextSource(const App *app);
// The library shown sorted and laid out again (after its shelf changed), the book selected kept.
void appResort(App *app);
// The network folder's library (app_netlib.c).
void appNetStart(App *app);   // Its shelf read from the card (shelf-network.txt), as last seen.
void appNetForget(App *app);  // Another folder, or none: the old one's library goes.
void appNetScan(App *app, uint64_t now_ns);  // Looks through the folder for its books.
// While it's looked through: true, and how many books so far.
bool appNetScanning(int *found);
void appNetUpdate(App *app, uint64_t now_ns);
// A network book (shelf index) to open: true with the card's copy in local when it's there
// (the same size); false after starting its copy (it opens once copied).
bool appNetReady(App *app, int book, char *local, size_t size, uint64_t now_ns);
void appNetCancelCopy(void);
// The network folder's shelf, which ever is shown.
Shelf *appNetShelf(App *app);
// A book being copied to the card to be opened (book: its index in the shelf shown).
void appCopyStarted(App *app, int book, const char *name, const char *local);
// The network started while a folder or a server is set (app_network.c).
void appNetworkUse(void);
// True for a source that's set up (the card always is).
bool appSourceSet(int tab);

// Free books (app_free.c).
void appFreeEntered(App *app, uint64_t now_ns);   // Its first list, the first time.
void appFreeShow(App *app, int list, uint64_t now_ns);
void appFreeStepList(App *app, int dir, uint64_t now_ns);  // L and R: the other lists.
void appFreeSearch(App *app, uint64_t now_ns);    // Y: the keyboard, then the search.
void appFreeLookAgain(App *app, uint64_t now_ns);
void appFreeUpdate(App *app, uint64_t now_ns);
void appFreeOpen(App *app, int order_index, uint64_t now_ns);  // Downloaded to the card, then opened.
bool appFreeLoading(void);
const char *appFreeListName(const App *app);
// The + menu's line for searching the catalog Y would search ("Search textos.info"), translated.
const char *appFreeSearchLine(const App *app);
// What to try when the last search found nothing, for its catalog's language, translated.
const char *appFreeSearchHint(const App *app);
// The + menu's line for the list L/R would show next ("Show Standard Ebooks' newest"), translated.
const char *appFreeNextListLine(const App *app);
// A book of the card's books folder opened, the card's library shown (a free book downloaded).
void appOpenCardBook(App *app, const char *path, uint64_t now_ns);
// The network started for Free books too (app_network.c).
void appNetworkWantFree(void);

// The news' showcase, as wide as the screen: cards across, a card's picture's width and height
// (16:9), a row's height, and the lead story's height.
int appNewsColumns(void);
void appNewsCardSize(int *w, int *picture_h);
int appNewsRowH(void);
int appNewsLeadH(void);
bool appNewsShowcase(const App *app);  // The news shown as a showcase (not as a list).

// News (app_news.c): RSS and Atom feeds' articles, read as books.
void appNewsStart(App *app);     // The feeds and their shelf read from the card, what was kept shown.
bool appNewsSet(void);           // A feed is followed (X goes to News).
// An article in the library's order: present, of the feed shown (or all of them).
bool appNewsShown(const App *app, const ShelfBook *b);
void appNewsEntered(App *app, uint64_t now_ns);   // Checked once a session, when News is first shown.
void appNewsRefresh(App *app, uint64_t now_ns);   // Y: every feed asked for its new articles.
void appNewsUpdate(App *app, uint64_t now_ns);
bool appNewsChecking(int *done, int *count);
void appNewsStepFeed(App *app, int dir);          // L and R: all the feeds, or one.
void appNewsMarkAllRead(App *app, uint64_t now_ns);
void appNewsFull(App *app, uint64_t now_ns);      // The open article's whole page, from its site.
bool appNewsReading(const App *app);              // An article is open (and has a link).
// An article (shelf index) to open: true with its page on the card in local.
bool appNewsReady(App *app, int book, char *local, size_t size, uint64_t now_ns);
const char *appNewsFeedName(const App *app);      // The feed shown ("All the feeds"), translated.
const char *appNewsNextFeedLine(const App *app);  // The + menu's "Show <next feed>", translated.
bool appNewsActivate(App *app, SettingRow row, int dir, uint64_t now_ns);
void appNewsFeedSettings(const App *app);
// Settings > News, its cursor on "Add a feed" (the welcome's News).
void appNewsOpenSettings(App *app);
// The network started for the news too (app_network.c).
void appNetworkWantNews(void);

// The book server's library and Settings page (app_server.c).
Shelf *appServerShelf(App *app);
void appServerStart(App *app);   // Its account and shelf read from the card.
void appServerForget(App *app);
void appServerScan(App *app, uint64_t now_ns);
bool appServerScanning(int *found);
void appServerUpdate(App *app, uint64_t now_ns);
void appServerTestUpdate(App *app, uint64_t now_ns);
bool appServerReady(App *app, int book, char *local, size_t size, uint64_t now_ns);
bool appServerActivate(App *app, SettingRow row, uint64_t now_ns);
void appServerFeedSettings(void);
// The copy's share done (0..1), -1 if its size isn't known; done and total in bytes.
float appNetCopyProgress(int64_t *done, int64_t *total);
void appOpenBook(App *app, int order_index, uint64_t now_ns);
void appCloseBook(App *app);

// The book open made again from path (its page changed: an article's pictures came), where it's read,
// or (at_start) from its start.
void appReopenBook(App *app, const char *path, bool at_start);

// app_scroll.c: a news article scrolled as a web page.
bool appScrollView(const App *app);    // Reading an article that way.
int appScrollStrip(void);              // A strip's height (layout units): the screen's.
int appScrollStripY(const App *app, int page);  // Where a strip's top is on screen.
void appScrollTo(App *app, float y);
void appScrollBy(App *app, float dy);
void appScrollOpened(App *app, int start_page);  // Laid out (again): at the strip it starts at.
void appScrollTick(App *app, uint64_t now_ns);   // Each frame: a drag let go glides on.
void appScrollKept(App *app, int page, const DocImage *image);  // A strip come (the last: where it ends).
bool appScrollButtons(App *app, uint64_t pressed, uint64_t now_ns);  // True if a button scrolled it.
float appScrollFraction(const App *app);  // How far down, 0..1.

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

// app_zoom.c: zoom on fixed pages (PDF, comics), one page at a time.
bool appZoomable(const App *app);
bool appZoomed(const App *app);
void appZoomTo(App *app, float zoom, float cx, float cy, uint64_t now_ns);
void appZoomStep(App *app, int dir, uint64_t now_ns);  // Up (+1) or down (-1) a step.
void appZoomPan(App *app, float dx, float dy, uint64_t now_ns);         // A finger's move.
void appZoomPanStep(App *app, int dx, int dy, uint64_t now_ns);         // A stick's push.
void appZoomToStart(App *app);   // A new page: its top left, as it's read.
void appZoomReset(App *app);
void appZoomTick(App *app, uint64_t now_ns);  // Each frame: sharp once it rests.
// Two fingers on the screen, each frame (count: how many touch; the first two, turned with the
// screen). True while they own the touch: from two fingers down until every finger is up.
bool appZoomTouches(App *app, int count, float x0, float y0, float x1, float y1, uint64_t now_ns);
// The page rendered again at the zoom, keeping the pages shown until the sharp ones come.
void appRenderAgain(App *app);
// The sharp part of a zoomed page to draw over it, if there is one finer than the page.
const PagePart *appPagePart(const App *app, int page);
// Asks for the parts on screen of the zoomed pages shown, once the view rests (app_zoom.c).
void appAskParts(App *app, uint64_t now_ns);
// Drops the parts kept (a new book, or closed).
void appForgetParts(App *app);
// Where a fixed page fits on screen at zoom 1 (layout units); false if it isn't here.
bool appFitRect(const App *app, int page, int *x, int *y, int *w, int *h);
// A page's image, of this layout or (a fixed page) one before, its size in pixels and the zoom it was made at.
bool appPageImageZoom(const App *app, int page, float *w, float *h, float *zoom);
// Touch coordinates as the panel reports them, turned with the screen (as appHandleTouch does).
void appTurnPoint(int *x, int *y);

// True when page's image is of the layout of the moment (not one from before, scaled).
bool appPageCurrent(const App *app, int page);
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
// The menu's groups (a line between them): the book's, the library's, then Settings and Exit.
int appMenuGroup(MenuItem item);
// The chapter the page is in (-1 if none), for the bars.
int appChapterOf(const App *app, int page);
// Rows the contents panel shows, for the screen's height.
int appContentsRows(void);
// The grid's left edge (the first cover's x) and the height of the library's view.
int appLibraryLeft(const App *app);
int appLibraryViewH(void);
