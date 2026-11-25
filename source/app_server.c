// The book server's library (an OPDS catalog): a shelf of its own
// (shelf-server.txt), shown as the card's when X comes to it. Its books are
// found by walking the catalog (netjob.c), their covers are the server's own
// thumbnails (opdsnet.c, kept in server-covers.txt), and a book opened is
// downloaded to the card first (sdmc:/books/.opds/...). Also its Settings
// page: address, user and password, test, forget.
#include "app.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "keyboard_system.h"
#include "lang.h"
#include "netjob.h"
#include "opdsnet.h"
#include "reading.h"

#define SERVER_SHELF_FILE "shelf-server.txt"
#define SERVER_COVERS_FILE "server-covers.txt"
#define TEST_TEXT_MAX 256
#define COVER_LINE_MAX 2200

static void pathIn(const App *app, const char *name, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", app->folder, name);
}

Shelf *appServerShelf(App *app)
{
    return app->lib_tab == LIB_SERVER ? &app->shelf : &app->stash[LIB_SERVER];
}

// --- The thumbnails' addresses, kept between sessions ---

static void loadCovers(const App *app)
{
    char file[APP_FOLDER_MAX + 32], line[COVER_LINE_MAX];
    pathIn(app, SERVER_COVERS_FILE, file, sizeof(file));
    FILE *f = fopen(file, "r");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *tab = strchr(line, '\t');
        if (tab) {
            *tab = '\0';
            opdsRememberCover(line, tab + 1);
        }
    }
    fclose(f);
}

static void saveCovers(const App *app, const OpdsBook *books, const char (*paths)[SHELF_PATH_MAX], int count)
{
    char file[APP_FOLDER_MAX + 32];
    pathIn(app, SERVER_COVERS_FILE, file, sizeof(file));
    FILE *f = fopen(file, "w");
    if (!f)
        return;
    for (int i = 0; i < count; i++)
        if (books[i].cover[0])
            fprintf(f, "%s\t%s\n", paths[i], books[i].cover);
    fclose(f);
}

static void saveServerShelf(App *app)
{
    char file[APP_FOLDER_MAX + 32];
    pathIn(app, SERVER_SHELF_FILE, file, sizeof(file));
    shelfSave(appServerShelf(app), file);
}

// --- The library ---

void appServerStart(App *app)
{
    char file[APP_FOLDER_MAX + 32];
    pathIn(app, OPDS_ACCOUNT_FILE, file, sizeof(file));
    opdsLoadAccount(file);
    readingSetCoverFetcher(opdsFetchCover);
    if (!opdsConfigured())
        return;
    pathIn(app, SERVER_SHELF_FILE, file, sizeof(file));
    Shelf *s = &app->stash[LIB_SERVER];
    shelfLoad(s, file);
    for (int i = 0; i < s->count; i++)  // As last seen, until the catalog is walked again.
        s->items[i].present = true;
    loadCovers(app);
}

void appServerForget(App *app)
{
    if (app->lib_tab == LIB_SERVER)
        appLibTab(app, LIB_CARD, 0);
    shelfFree(&app->stash[LIB_SERVER]);
    app->stash_cursor[LIB_SERVER] = app->stash_scroll[LIB_SERVER] = 0;
    app->server_scanned = false;
    app->server_problem[0] = '\0';
    opdsForgetCovers();
    char file[APP_FOLDER_MAX + 32];
    pathIn(app, SERVER_SHELF_FILE, file, sizeof(file));
    remove(file);
    pathIn(app, SERVER_COVERS_FILE, file, sizeof(file));
    remove(file);
}

void appServerScan(App *app, uint64_t now_ns)
{
    DavAccount a;
    opdsGetAccount(&a);
    char catalog[DAV_ADDRESS_MAX];
    snprintf(catalog, sizeof(catalog), "%s", a.address);
    memset(&a, 0, sizeof(a));
    (void)now_ns;
    if (!catalog[0])
        return;
    app->server_problem[0] = '\0';
    if (netJobServerScan(catalog))
        app->server_scanned = true;
}

bool appServerScanning(int *found)
{
    return netJobServerScanning(found);
}

// The catalog's books: present, with their titles, authors, genres and thumbnails.
static void takeScan(App *app, uint64_t now_ns)
{
    OpdsBook *books;
    int count;
    bool ok;
    char why[NETJOB_WHY_MAX];
    if (!netJobTakeServerScan(&books, &count, &ok, why, sizeof(why)))
        return;
    if (!ok) {
        snprintf(app->server_problem, sizeof(app->server_problem), "%s", why);
        if (app->lib_tab == LIB_SERVER)
            appNotify(app, NOTICE_ERROR, why, now_ns);
        free(books);
        return;
    }
    char (*paths)[SHELF_PATH_MAX] = calloc((size_t)(count > 0 ? count : 1), sizeof(*paths));
    Shelf *s = appServerShelf(app);
    for (int i = 0; i < s->count; i++)
        s->items[i].present = false;
    int kept = 0;
    for (int i = 0; paths && i < count; i++) {
        char path[SHELF_PATH_MAX];
        int n = snprintf(path, sizeof(path), "%s%s#%s", OPDS_PREFIX, books[i].acquire, books[i].ext);
        if (n <= 0 || (size_t)n >= sizeof(path))
            continue;  // A link too long to keep.
        ShelfBook *b = shelfFind(s, path);
        if (!b)
            b = shelfAdd(s, path);
        if (!b)
            break;
        b->present = true;
        b->size = books[i].size;
        if (!b->title[0] || b->percent < 0)  // The catalog's words until the book, opened, says its own.
            shelfSetInfo(b, books[i].title, books[i].author);
        shelfSetDetails(b, books[i].genre, "");
        opdsRememberCover(path, books[i].cover);
        books[kept] = books[i];
        snprintf(paths[kept++], SHELF_PATH_MAX, "%s", path);
    }
    if (paths)
        saveCovers(app, books, (const char (*)[SHELF_PATH_MAX])paths, kept);
    free(paths);
    free(books);
    if (app->lib_tab == LIB_SERVER)
        appResort(app);
    saveServerShelf(app);
}

bool appServerReady(App *app, int book, char *local, size_t size, uint64_t now_ns)
{
    const ShelfBook *b = &app->shelf.items[book];
    char link[OPDS_URL_MAX];
    if (!opdsLinkOf(b->path, link, sizeof(link)) || !opdsLocalPath(app->books_folder, b->path, b->title, local, size)) {
        appNotify(app, NOTICE_ERROR, T("That book's name is too long to keep on the card."), now_ns);
        return false;
    }
    struct stat st;
    if (stat(local, &st) == 0 && st.st_size > 0 && (b->size <= 0 || (int64_t)st.st_size == b->size))
        return true;
    if (!netJobCopyLink(link, local, b->size)) {
        appNotify(app, NOTICE_INFO, T("A book is being copied already."), now_ns);
        return false;
    }
    appCopyStarted(app, book, b->title, local);
    return false;
}

void appServerUpdate(App *app, uint64_t now_ns)
{
    takeScan(app, now_ns);
}

// --- Its Settings page ---

static pthread_mutex_t g_test_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_testing, g_tested, g_test_ok;
static char g_test_text[TEST_TEXT_MAX];

void appServerFeedSettings(void)
{
    char shown[DAV_ADDRESS_MAX];
    DavAccount a;
    opdsGetAccount(&a);
    davShownAddress(a.address, shown, sizeof(shown));
    settingsSetServer(shown, a.user);
    memset(&a, 0, sizeof(a));
}

static void save(App *app, const DavAccount *a, uint64_t now)
{
    DavAccount before;
    opdsGetAccount(&before);
    bool another = strcmp(before.address, a->address) != 0;
    memset(&before, 0, sizeof(before));
    if (another)  // Another server: the old one's library goes.
        appServerForget(app);
    opdsUseAccount(a);
    char path[APP_FOLDER_MAX + 32];
    pathIn(app, OPDS_ACCOUNT_FILE, path, sizeof(path));
    if (!opdsSaveAccount(path))
        appNotify(app, NOTICE_ERROR, T("It couldn't be written to the card."), now);
    appNetworkUse();
    appServerFeedSettings();
}

static void typeAddress(App *app, uint64_t now)
{
    DavAccount a;
    opdsGetAccount(&a);
    char shown[DAV_ADDRESS_MAX], text[DAV_ADDRESS_MAX], address[DAV_ADDRESS_MAX];
    davShownAddress(a.address, shown, sizeof(shown));
    SystemKeyboardResult result = systemKeyboardInput(
        T("The server's address: 192.168.0.10:8083 (Calibre-Web), 192.168.0.10:25600 (Komga), or its OPDS address"),
        shown, text, sizeof(text), sizeof(text) - 10);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now);
    } else if (result == SYSTEM_KEYBOARD_OK &&
               (!davNormalizeAddress(text, address, sizeof(address)) || davProtocolOf(address) != DAV_WEBDAV)) {
        appNotify(app, NOTICE_ERROR, T("That isn't a server's address: 192.168.0.10:8083, http:// or https://."), now);
    } else if (result == SYSTEM_KEYBOARD_OK) {
        if (!davSameServer(address, a.address)) {  // Another server: the old one's password isn't sent there.
            a.user[0] = '\0';
            memset(a.password, 0, sizeof(a.password));
        }
        memcpy(a.address, address, sizeof(a.address));
        save(app, &a, now);
        appNotify(app, NOTICE_DONE,
                  T("Server set: add your user and password if it asks for them, then test the connection."), now);
    }
    memset(&a, 0, sizeof(a));
}

static void typeAccount(App *app, uint64_t now)
{
    DavAccount a;
    opdsGetAccount(&a);
    if (!a.address[0]) {
        appNotify(app, NOTICE_INFO, T("Enter the server's address first."), now);
        memset(&a, 0, sizeof(a));
        return;
    }
    char user[DAV_TEXT_MAX], password[DAV_TEXT_MAX] = "";
    SystemKeyboardResult result =
        systemKeyboardInput(T("User name (empty: none)"), a.user, user, sizeof(user), sizeof(user) - 1);
    if (result == SYSTEM_KEYBOARD_OK && user[0])
        result = systemKeyboardPassword(T("Password"), password, sizeof(password), sizeof(password) - 1);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now);
    } else if (result == SYSTEM_KEYBOARD_OK) {
        snprintf(a.user, sizeof(a.user), "%s", user);
        if (davSetPassword(&a, password)) {
            save(app, &a, now);
            if (a.user[0])
                appNotify(app, NOTICE_DONE,
                          T("Saved: the password is only encoded on the card. Use a user that can only read the books."),
                          now);
        } else {
            appNotify(app, NOTICE_ERROR, T("That password can't be used."), now);
        }
    }
    memset(password, 0, sizeof(password));
    memset(&a, 0, sizeof(a));
}

// The test finds the catalog under a plain address (/opds, /opds/v1.2/catalog...) and keeps it.
static void *testThread(void *arg)
{
    (void)arg;
    char text[TEST_TEXT_MAX], catalog[DAV_ADDRESS_MAX], why[TEST_TEXT_MAX];
    DavAccount a;
    opdsGetAccount(&a);
    bool ok = opdsFindCatalog(a.address, catalog, sizeof(catalog), why, sizeof(why));
    if (ok && strcmp(catalog, a.address) != 0) {
        snprintf(a.address, sizeof(a.address), "%s", catalog);
        opdsUseAccount(&a);  // Saved by the screen's thread when the result's taken.
    }
    memset(&a, 0, sizeof(a));
    if (ok)
        ok = opdsTest(text, sizeof(text));
    else
        snprintf(text, sizeof(text), "%s", why);
    pthread_mutex_lock(&g_test_lock);
    snprintf(g_test_text, sizeof(g_test_text), "%s", text);
    g_test_ok = ok;
    g_testing = false;
    g_tested = true;
    pthread_mutex_unlock(&g_test_lock);
    return NULL;
}

static void test(App *app, uint64_t now)
{
    if (!opdsConfigured()) {
        appNotify(app, NOTICE_INFO, T("Enter the server's address first."), now);
        return;
    }
    pthread_mutex_lock(&g_test_lock);
    bool busy = g_testing;
    pthread_t thread;
    if (!busy)
        g_testing = pthread_create(&thread, NULL, testThread, NULL) == 0;
    bool started = !busy && g_testing;
    pthread_mutex_unlock(&g_test_lock);
    if (started)
        pthread_detach(thread);
    appNotify(app, NOTICE_INFO, busy || started ? T("Testing the connection...") : T("The network can't be used."), now);
}

static void forget(App *app, uint64_t now)
{
    DavAccount none = {0};
    save(app, &none, now);
    appNotify(app, NOTICE_DONE, T("The book server is forgotten."), now);
}

bool appServerActivate(App *app, SettingRow row, uint64_t now)
{
    switch (row) {
    case SETTING_SRV_ADDRESS: typeAddress(app, now); return true;
    case SETTING_SRV_ACCOUNT: typeAccount(app, now); return true;
    case SETTING_SRV_TEST: test(app, now); return true;
    case SETTING_SRV_FORGET: forget(app, now); return true;
    default: return false;
    }
}

void appServerTestUpdate(App *app, uint64_t now)
{
    char text[TEST_TEXT_MAX] = "";
    bool ok = false;
    pthread_mutex_lock(&g_test_lock);
    if (g_tested) {
        ok = g_test_ok;
        snprintf(text, sizeof(text), "%s", g_test_text);
        g_tested = false;
    }
    pthread_mutex_unlock(&g_test_lock);
    if (!text[0])
        return;
    if (ok) {  // The catalog's address, as found: kept.
        char path[APP_FOLDER_MAX + 32];
        pathIn(app, OPDS_ACCOUNT_FILE, path, sizeof(path));
        opdsSaveAccount(path);
        appServerFeedSettings();
    }
    appNotify(app, ok ? NOTICE_DONE : NOTICE_ERROR, text, now);
}
