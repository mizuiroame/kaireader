// The library's Network tab: the network folder's folders and books (listed
// off the screen's thread, netjob.c), and a book chosen copied to the card
// (sdmc:/books/Network/..., in plain names), then opened from there. A book
// copied before, of the same size, opens at once.
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "buttons.h"
#include "davnet.h"
#include "gfx.h"
#include "lang.h"
#include "netjob.h"
#include "netlist.h"

int appNetRows(void)
{
    int rows = (gfxLayoutHeight() - NET_LIST_TOP - FOOTER_H - 8) / NET_ROW_H;
    return rows < 1 ? 1 : rows;
}

void appNetForget(App *app)
{
    free(app->net_entries);
    app->net_entries = NULL;
    app->net_count = 0;
    app->net_cursor = app->net_first = 0;
    app->net_location[0] = '\0';
    app->net_problem[0] = '\0';
    app->net_loading = false;
}

static void keepCursorShown(App *app)
{
    int rows = appNetRows(), n = (int)app->net_count;
    app->net_cursor = app->net_cursor < 0 ? 0 : app->net_cursor >= n ? (n > 0 ? n - 1 : 0) : app->net_cursor;
    if (app->net_cursor < app->net_first)
        app->net_first = app->net_cursor;
    if (app->net_cursor >= app->net_first + rows)
        app->net_first = app->net_cursor - rows + 1;
    int most = n - rows > 0 ? n - rows : 0;
    app->net_first = app->net_first < 0 ? 0 : app->net_first > most ? most : app->net_first;
}

// Lists a folder; select is the name to put the cursor on once it's here ("" the first).
static void openFolder(App *app, const char *location, const char *select, uint64_t now_ns)
{
    if (!appNetworkEnsurePassword(app, now_ns))
        return;
    snprintf(app->net_asked, sizeof(app->net_asked), "%s", location);
    snprintf(app->net_select, sizeof(app->net_select), "%s", select);
    app->net_loading = true;
    app->net_problem[0] = '\0';
    netJobList(location);
}

void appLibTab(App *app, int tab, uint64_t now_ns)
{
    tab = tab < 0 ? 0 : tab > 1 ? 1 : tab;
    if (app->lib_tab == tab)
        return;
    app->lib_tab = tab;
    if (tab == 1 && davConfigured() && !app->net_location[0] && !app->net_loading)
        openFolder(app, DAV_ROOT, "", now_ns);
}

void appNetLookAgain(App *app, uint64_t now_ns)
{
    if (!davConfigured())
        return;
    const char *at = app->net_location[0] ? app->net_location : DAV_ROOT;
    char select[DAV_NAME_MAX] = "";
    if (app->net_cursor >= 0 && (size_t)app->net_cursor < app->net_count)
        snprintf(select, sizeof(select), "%s", app->net_entries[app->net_cursor].name);
    char location[DAV_ADDRESS_MAX];
    snprintf(location, sizeof(location), "%s", at);
    openFolder(app, location, select, now_ns);
}

static int64_t fileSize(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (int64_t)st.st_size : -1;
}

// A book of the folder: opened from the card if it's there already, else copied first.
static void openBook(App *app, const DavEntry *e, uint64_t now_ns)
{
    char location[DAV_ADDRESS_MAX], local[NET_LOCAL_MAX];
    if (!netChild(app->net_location, e->name, location, sizeof(location)) ||
        !netLocalPath(app->books_folder, location, local, sizeof(local))) {
        appNotify(app, NOTICE_ERROR, T("That book's name is too long to keep on the card."), now_ns);
        return;
    }
    int64_t have = fileSize(local);
    if (have >= 0 && (e->size < 0 || have == e->size)) {
        appOpenPath(app, local, now_ns);
        return;
    }
    if (!appNetworkEnsurePassword(app, now_ns))
        return;
    if (!netJobCopy(location, local, e->size)) {
        appNotify(app, NOTICE_INFO, T("A book is being copied already."), now_ns);
        return;
    }
    snprintf(app->net_copying, sizeof(app->net_copying), "%s", e->name);
    snprintf(app->net_copy_local, sizeof(app->net_copy_local), "%s", local);
}

void appNetPick(App *app, int index, uint64_t now_ns)
{
    if (index < 0 || (size_t)index >= app->net_count || app->net_copying[0])
        return;
    app->net_cursor = index;
    const DavEntry *e = &app->net_entries[index];
    if (e->is_dir) {
        char location[DAV_ADDRESS_MAX];
        if (netChild(app->net_location, e->name, location, sizeof(location)))
            openFolder(app, location, "", now_ns);
    } else {
        openBook(app, e, now_ns);
    }
}

// Up a folder, the cursor on the one left; at the top, back to the books on the card.
static void goUp(App *app, uint64_t now_ns)
{
    char parent[DAV_ADDRESS_MAX];
    if (app->net_location[0] && netParent(app->net_location, parent, sizeof(parent)))
        openFolder(app, parent, netLastName(app->net_location), now_ns);
    else
        appLibTab(app, 0, now_ns);
}

void appNetMove(App *app, int by)
{
    if (app->net_count == 0)
        return;
    app->net_cursor += by;
    keepCursorShown(app);
}

void appNetScroll(App *app, int rows)
{
    int most = (int)app->net_count - appNetRows();
    app->net_first += rows;
    app->net_first = app->net_first > most ? most : app->net_first;
    app->net_first = app->net_first < 0 ? 0 : app->net_first;
    int last = app->net_first + appNetRows() - 1;
    app->net_cursor = app->net_cursor < app->net_first ? app->net_first : app->net_cursor > last ? last : app->net_cursor;
}

void appNetButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if (app->net_copying[0]) {  // Copying: only B, to stop it.
        if (pressed & HidNpadButton_B)
            netJobCancelCopy();
        return;
    }
    if (pressed & HidNpadButton_B)
        goUp(app, now_ns);
    else if (pressed & HidNpadButton_Y)
        appNetLookAgain(app, now_ns);
    else if (!davConfigured())
        return;
    else if (pressed & BUTTONS_UP)
        appNetMove(app, -1);
    else if (pressed & BUTTONS_DOWN)
        appNetMove(app, 1);
    else if (pressed & (HidNpadButton_L | HidNpadButton_Left))
        appNetMove(app, -appNetRows());
    else if (pressed & (HidNpadButton_R | HidNpadButton_Right))
        appNetMove(app, appNetRows());
    else if (pressed & HidNpadButton_A)
        appNetPick(app, app->net_cursor, now_ns);
}

// A listing come: shown, the cursor on the name asked for.
static void takeListing(App *app, uint64_t now_ns)
{
    char location[DAV_ADDRESS_MAX], why[NETJOB_WHY_MAX];
    DavEntry *entries;
    size_t count;
    bool ok;
    if (!netJobTakeList(location, sizeof(location), &entries, &count, &ok, why, sizeof(why)))
        return;
    if (strcmp(location, app->net_asked) != 0) {  // An older one.
        free(entries);
        return;
    }
    app->net_loading = false;
    if (!ok) {
        snprintf(app->net_problem, sizeof(app->net_problem), "%s", why);
        if (!app->net_location[0])  // Nothing shown yet: the problem is shown in its place.
            snprintf(app->net_location, sizeof(app->net_location), "%s", location);
        else
            appNotify(app, NOTICE_ERROR, why, now_ns);
        return;
    }
    free(app->net_entries);
    app->net_entries = entries;
    app->net_count = count;
    app->net_problem[0] = '\0';
    snprintf(app->net_location, sizeof(app->net_location), "%s", location);
    app->net_cursor = app->net_first = 0;
    for (size_t i = 0; i < count && app->net_select[0]; i++)
        if (!strcmp(entries[i].name, app->net_select))
            app->net_cursor = (int)i;
    keepCursorShown(app);
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
    if (state == NETJOB_DONE)
        appOpenPath(app, app->net_copy_local, now_ns);
    else if (why[0])
        appNotify(app, NOTICE_ERROR, why, now_ns);
    else
        appNotify(app, NOTICE_INFO, T("Copy stopped."), now_ns);
}

void appNetUpdate(App *app, uint64_t now_ns)
{
    takeListing(app, now_ns);
    followCopy(app, now_ns);
}

float appNetCopyProgress(int64_t *done, int64_t *total)
{
    char why[NETJOB_WHY_MAX];
    netJobCopyState(done, total, why, sizeof(why));
    return *total > 0 ? (float)*done / (float)*total : -1.0f;
}
