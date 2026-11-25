// Free books: public-domain catalogs (catalogs.h) browsed as a library, after
// the card's, the network folder's and the server's (X). L and R change the
// list (Project Gutenberg's most popular, Standard Ebooks' newest, textos.info's
// most popular in Spanish, Ebooks libres et gratuits' newest in French, unglue.it's
// most popular under Creative Commons, a search), Y searches the catalog of the
// list shown, and the next page comes as the end nears. A
// book chosen is downloaded to the card's books folder (sdmc:/books/Free/,
// "Title - Author.epub"), then opened there: it's one of the card's books from
// then on. Nothing is asked of the catalogs until Free books is opened.
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "catalogs.h"
#include "keyboard_system.h"
#include "lang.h"
#include "netjob.h"
#include "opdsnet.h"

#define FREE_FOLDER "Free"
#define MORE_LINES 2  // The next page is asked for when the cursor is this many lines from the end.

// The Free books' shelf: the one shown, or kept aside while another library is.
static Shelf *freeShelf(App *app)
{
    return app->lib_tab == LIB_FREE ? &app->shelf : &app->stash[LIB_FREE];
}

// No books shown: a new list's first page is on its way (or the list couldn't be read).
static void emptyList(App *app)
{
    shelfFree(freeShelf(app));
    if (app->lib_tab == LIB_FREE) {
        app->cursor = 0;
        app->scroll = 0;
        appResort(app);
    }
}

static void ask(App *app, const char *url, bool more)
{
    snprintf(app->free_asked, sizeof(app->free_asked), "%s", url);
    app->free_more = more;
    app->free_loading = true;
    app->free_problem[0] = '\0';
    netJobCatalogPage(url);
}

void appFreeShow(App *app, int list, uint64_t now_ns)
{
    char url[OPDS_URL_MAX];
    if (!catalogUrl((CatalogList)list, (CatalogList)app->free_search_in, app->free_query, url, sizeof(url))) {
        if (list == CATALOG_SEARCH)
            appFreeSearch(app, now_ns);
        return;
    }
    app->free_list = list;
    app->free_next[0] = '\0';
    emptyList(app);  // The old list's books never stay under the new list's name.
    appNetworkWantFree();
    ask(app, url, false);
}

// The list dir steps from the one shown: round them all, the search once there's been one.
static int stepped(const App *app, int dir)
{
    int count = app->free_query[0] ? CATALOG_COUNT : CATALOG_SEARCH;
    return ((app->free_list + dir) % count + count) % count;
}

void appFreeStepList(App *app, int dir, uint64_t now_ns)
{
    appFreeShow(app, stepped(app, dir), now_ns);
}

const char *appFreeNextListLine(const App *app)
{
    return T(catalogShowLine((CatalogList)stepped(app, 1)));
}

// The catalog Y searches: the shown list's, or on a search, the one searched.
static CatalogList searchedIn(const App *app)
{
    return catalogSearchOf((CatalogList)app->free_list, (CatalogList)app->free_search_in);
}

const char *appFreeSearchLine(const App *app)
{
    return T(catalogSearchLine(searchedIn(app)));
}

const char *appFreeSearchHint(const App *app)
{
    return T(catalogSearchHint((CatalogList)app->free_search_in));
}

void appFreeSearch(App *app, uint64_t now_ns)
{
    char words[CATALOG_QUERY_MAX];
    CatalogList in = searchedIn(app);
    SystemKeyboardResult result = systemKeyboardInput(T(catalogSearchPrompt(in)), app->free_query, words,
                                                      sizeof(words), sizeof(words) - 1);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now_ns);
        return;
    }
    if (result != SYSTEM_KEYBOARD_OK || !words[0])
        return;
    snprintf(app->free_query, sizeof(app->free_query), "%s", words);
    app->free_search_in = in;
    appFreeShow(app, CATALOG_SEARCH, now_ns);
}

const char *appFreeListName(const App *app)
{
    if (app->free_list == CATALOG_SEARCH)
        return T(catalogSearchName((CatalogList)app->free_search_in));
    return T(catalogName((CatalogList)app->free_list));
}

// A page come: its books shown (after the ones before, for a next page).
static void takePage(App *app, uint64_t now_ns)
{
    char url[OPDS_URL_MAX], next[OPDS_URL_MAX], why[NETJOB_WHY_MAX];
    OpdsBook *books;
    int count;
    bool ok;
    if (!netJobTakeCatalogPage(url, sizeof(url), &books, &count, next, sizeof(next), &ok, why, sizeof(why)))
        return;
    if (strcmp(url, app->free_asked) != 0) {  // An older one.
        free(books);
        return;
    }
    app->free_loading = false;
    if (!ok) {
        snprintf(app->free_problem, sizeof(app->free_problem), "%s", why);
        if (app->lib_tab == LIB_FREE && app->free_more)
            appNotify(app, NOTICE_ERROR, why, now_ns);
        free(books);
        return;
    }
    if (!app->free_more)
        emptyList(app);
    Shelf *s = freeShelf(app);
    for (int i = 0; i < count; i++) {
        char path[SHELF_PATH_MAX];
        int n = snprintf(path, sizeof(path), "%s%s#%s", OPDS_PREFIX, books[i].acquire, books[i].ext);
        if (n <= 0 || (size_t)n >= sizeof(path) || shelfFind(s, path))
            continue;
        ShelfBook *b = shelfAdd(s, path);
        if (!b)
            break;
        b->size = books[i].size;
        shelfSetInfo(b, books[i].title, books[i].author);
        shelfSetDetails(b, books[i].genre, books[i].author_sort);
        opdsRememberCover(path, books[i].cover);
    }
    free(books);
    snprintf(app->free_next, sizeof(app->free_next), "%s", next);
    if (app->lib_tab == LIB_FREE)
        appResort(app);
}

void appFreeUpdate(App *app, uint64_t now_ns)
{
    takePage(app, now_ns);
    // Near the end of what's shown: the next page.
    if (app->lib_tab != LIB_FREE || app->free_loading || !app->free_next[0] || app->view != VIEW_LIBRARY)
        return;
    if (app->cursor >= app->order_count - MORE_LINES * (app->columns > 0 ? app->columns : 1))
        ask(app, app->free_next, true);
}

void appFreeEntered(App *app, uint64_t now_ns)
{
    if (!app->free_asked[0])  // The first time: the most popular.
        appFreeShow(app, CATALOG_POPULAR, now_ns);
}

void appFreeLookAgain(App *app, uint64_t now_ns)
{
    appFreeShow(app, app->free_list, now_ns);
}

bool appFreeLoading(void)
{
    return netJobCatalogLoading();
}

// The card's books folder: where a free book goes, by its title and author.
static bool localOf(const App *app, const ShelfBook *b, char *out, size_t size)
{
    OpdsBook named;
    memset(&named, 0, sizeof(named));
    snprintf(named.title, sizeof(named.title), "%s", b->title);
    snprintf(named.author, sizeof(named.author), "%s", b->author);
    const char *hash = strrchr(b->path, '#');
    snprintf(named.ext, sizeof(named.ext), "%s", hash && hash[1] == '.' ? hash + 1 : ".epub");
    char file[256];
    catalogFileName(&named, file, sizeof(file));
    int n = snprintf(out, size, "%s/%s/%s", app->books_folder, FREE_FOLDER, file);
    return n > 0 && (size_t)n < size;
}

void appFreeOpen(App *app, int order_index, uint64_t now_ns)
{
    if (order_index < 0 || order_index >= app->order_count)
        return;
    const ShelfBook *b = &app->shelf.items[app->order[order_index]];
    char local[NET_LOCAL_MAX], link[OPDS_URL_MAX];
    if (!localOf(app, b, local, sizeof(local)) || !opdsLinkOf(b->path, link, sizeof(link))) {
        appNotify(app, NOTICE_ERROR, T("That book's name is too long to keep on the card."), now_ns);
        return;
    }
    struct stat st;
    if (stat(local, &st) == 0 && st.st_size > 0) {  // Downloaded before: it's one of the card's books.
        appOpenCardBook(app, local, now_ns);
        return;
    }
    if (!netJobCopyLink(link, local, b->size)) {
        appNotify(app, NOTICE_INFO, T("A book is being copied already."), now_ns);
        return;
    }
    appCopyStarted(app, app->order[order_index], b->title, local);
}
