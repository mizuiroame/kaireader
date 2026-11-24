// The network folder's library: a shelf of its own (shelf-network.txt), shown
// as the card's is (grid or list, sorts, favorites) when X switches to it.
// Its books are found by looking through the folder (netjob.c), their covers
// and titles read over the network a few blocks at a time (netstream.c). A
// book opened is copied to the card first (sdmc:/books/.network/..., hidden
// from the card's library, in plain names), then opened from there; one
// copied before, of the same size, opens at once.
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "davnet.h"
#include "lang.h"
#include "netjob.h"
#include "netlist.h"

#define NET_SHELF_FILE "shelf-network.txt"

Shelf *appNetShelf(App *app)
{
    return app->lib_tab == 1 ? &app->shelf : &app->other_shelf;
}

static void shelfPath(const App *app, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", app->folder, NET_SHELF_FILE);
}

static void saveNetShelf(App *app)
{
    char file[APP_FOLDER_MAX + 32];
    shelfPath(app, file, sizeof(file));
    shelfSave(appNetShelf(app), file);
}

void appNetStart(App *app)
{
    app->net_open = -1;
    if (!davConfigured())
        return;
    char file[APP_FOLDER_MAX + 32];
    shelfPath(app, file, sizeof(file));
    shelfLoad(&app->other_shelf, file);
    for (int i = 0; i < app->other_shelf.count; i++)  // As last seen, until the folder is looked through.
        app->other_shelf.items[i].present = true;
}

void appNetForget(App *app)
{
    if (app->lib_tab == 1)
        appLibTab(app, 0, 0);
    shelfFree(&app->other_shelf);
    app->other_cursor = app->other_scroll = 0;
    app->net_scanned = false;
    app->net_problem[0] = '\0';
    char file[APP_FOLDER_MAX + 32];
    shelfPath(app, file, sizeof(file));
    remove(file);
}

void appNetScan(App *app, uint64_t now_ns)
{
    if (!davConfigured() || !appNetworkEnsurePassword(app, now_ns))
        return;
    app->net_problem[0] = '\0';
    if (netJobScan())
        app->net_scanned = true;
}

bool appNetScanning(int *found)
{
    return netJobScanning(found);
}

// The books found: present, with their sizes; the rest not any more (kept if read or favorites).
static void takeScan(App *app, uint64_t now_ns)
{
    NetBook *books;
    int count;
    bool ok;
    char why[NETJOB_WHY_MAX];
    if (!netJobTakeScan(&books, &count, &ok, why, sizeof(why)))
        return;
    if (!ok) {  // The books last seen stay shown.
        snprintf(app->net_problem, sizeof(app->net_problem), "%s", why);
        if (app->lib_tab == 1)
            appNotify(app, NOTICE_ERROR, why, now_ns);
        free(books);
        return;
    }
    Shelf *s = appNetShelf(app);
    for (int i = 0; i < s->count; i++)
        s->items[i].present = false;
    for (int i = 0; i < count; i++) {
        ShelfBook *b = shelfFind(s, books[i].location);
        if (!b)
            b = shelfAdd(s, books[i].location);
        if (!b)
            break;  // Full.
        b->present = true;
        if (books[i].size >= 0)
            b->size = books[i].size;
    }
    free(books);
    if (app->lib_tab == 1)
        appResort(app);
    saveNetShelf(app);
}

bool appNetReady(App *app, int book, char *local, size_t size, uint64_t now_ns)
{
    const ShelfBook *b = &app->shelf.items[book];
    if (!netLocalPath(app->books_folder, b->path, local, size)) {
        appNotify(app, NOTICE_ERROR, T("That book's name is too long to keep on the card."), now_ns);
        return false;
    }
    struct stat st;
    if (stat(local, &st) == 0 && (b->size <= 0 || (int64_t)st.st_size == b->size))
        return true;
    if (!appNetworkEnsurePassword(app, now_ns))
        return false;
    if (!netJobCopy(b->path, local, b->size)) {
        appNotify(app, NOTICE_INFO, T("A book is being copied already."), now_ns);
        return false;
    }
    shelfDisplayName(b, app->net_copying, sizeof(app->net_copying));
    snprintf(app->net_copy_local, sizeof(app->net_copy_local), "%s", local);
    app->net_open = book;
    return false;
}

void appNetCancelCopy(void)
{
    netJobCancelCopy();
}

// A copy ended: the book opened, or why not.
static void followCopy(App *app, uint64_t now_ns)
{
    if (!app->net_copying[0])
        return;
    int64_t done, total;
    char why[NETJOB_WHY_MAX];
    NetJobState state = netJobCopyState(&done, &total, why, sizeof(why));
    if (state == NETJOB_BUSY)
        return;
    netJobForgetCopy();
    app->net_copying[0] = '\0';
    int book = app->net_open;
    app->net_open = -1;
    if (state != NETJOB_DONE) {
        appNotify(app, why[0] ? NOTICE_ERROR : NOTICE_INFO, why[0] ? why : T("Copy stopped."), now_ns);
        return;
    }
    if (app->lib_tab != 1 || app->view != VIEW_LIBRARY)
        return;  // Switched away meanwhile: it's on the card now, and opens at once next time.
    for (int i = 0; i < app->order_count; i++)
        if (app->order[i] == book) {
            appOpenBook(app, i, now_ns);
            return;
        }
}

void appNetUpdate(App *app, uint64_t now_ns)
{
    takeScan(app, now_ns);
    followCopy(app, now_ns);
}

float appNetCopyProgress(int64_t *done, int64_t *total)
{
    char why[NETJOB_WHY_MAX];
    netJobCopyState(done, total, why, sizeof(why));
    return *total > 0 ? (float)*done / (float)*total : -1.0f;
}
