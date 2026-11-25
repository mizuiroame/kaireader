// The network for the previews: a made-up folder ("smb://NAS/Books/") whose
// books are the made-up ones in ./books (a "dav:/Novels/quiet.epub" opens
// books/quiet.epub), found at once, a copy that stays at 40%, and the
// account's Settings rows filled in. Stands in for netjob.c, davnet.c,
// netstream.c and app_network.c, which need the console's network.
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app.h"
#include "davnet.h"
#include "doc.h"
#include "keyboard_system.h"
#include "netjob.h"
#include "opdsnet.h"

bool g_preview_net_configured;
bool g_preview_server_configured;

// The made-up book server: some of the books, as a catalog would list them.
static const struct {
    const char *file, *title, *author, *genre;
} SERVED[] = {
    {"library3.epub", "Copper Evenings", "Mara Fennick", "Mystery"},
    {"library6.epub", "Harbor Lights", "Lio Arden", "Travel"},
    {"library8.epub", "The Last Ferry", "Ivo Castellan", "Adventure"},
    {"quiet.epub", "The Quiet Lighthouse", "Ana Example", "Sea Stories"},
    {"strip.cbz", "Strip", "", "Comics"},
};
static bool g_server_ready;

static const char *const FOUND[] = {
    "dav:/Novels/library2.epub", "dav:/Novels/library5.epub", "dav:/Novels/library7.epub",
    "dav:/Novels/old.epub",      "dav:/Comics/strip.cbz",     "dav:/Comics/wide.pdf",
    "dav:/Essays/tale.fb2",      "dav:/plain.pdf",
};

static bool g_scan_ready;
static NetJobState g_copy = NETJOB_IDLE;

bool davConfigured(void)
{
    return g_preview_net_configured;
}

void davGetAccount(DavAccount *out)
{
    memset(out, 0, sizeof(*out));
    if (g_preview_net_configured)
        snprintf(out->address, sizeof(out->address), "%s", "smb://NAS/Books/");
}

// "dav:/Novels/quiet.epub" -> "books/quiet.epub".
static void fixtureOf(const char *location, char *out, size_t size)
{
    const char *slash = strrchr(location, '/');
    snprintf(out, size, "books/%s", slash ? slash + 1 : location);
}

static fz_stream *openFixture(fz_context *ctx, const char *location)
{
    char path[256];
    fixtureOf(location, path, sizeof(path));
    return fz_open_file(ctx, path);
}

bool netJobScan(void)
{
    g_scan_ready = true;
    return true;
}

bool netJobScanning(int *found)
{
    *found = 0;
    return false;
}

bool netJobTakeScan(NetBook **books, int *count, bool *ok, char *why, size_t why_size)
{
    if (!g_scan_ready)
        return false;
    g_scan_ready = false;
    int n = (int)(sizeof(FOUND) / sizeof(FOUND[0]));
    *books = calloc((size_t)n, sizeof(**books));
    *count = 0;
    for (int i = 0; *books && i < n; i++) {
        char path[256];
        struct stat st;
        fixtureOf(FOUND[i], path, sizeof(path));
        if (stat(path, &st) != 0)
            continue;
        NetBook *b = &(*books)[(*count)++];
        snprintf(b->location, sizeof(b->location), "%s", FOUND[i]);
        b->size = (int64_t)st.st_size;
    }
    *ok = true;
    if (why_size)
        why[0] = '\0';
    return true;
}

bool netJobCopy(const char *location, const char *local, int64_t size)
{
    (void)location;
    (void)local;
    (void)size;
    g_copy = NETJOB_BUSY;
    return true;
}

void netJobCancelCopy(void)
{
    g_copy = NETJOB_FAILED;
}

NetJobState netJobCopyState(int64_t *done, int64_t *total, char *why, size_t why_size)
{
    *total = 4210000;
    *done = *total * 2 / 5;
    if (why_size)
        why[0] = '\0';
    return g_copy;
}

void netJobForgetCopy(void)
{
    g_copy = NETJOB_IDLE;
}

void netJobStop(void)
{
}

void appNetworkStart(App *app)
{
    (void)app;
    docSetRemoteOpener(openFixture);
    appNetworkFeedSettings();
}

void appNetworkExit(void)
{
}

bool appNetworkActivate(App *app, SettingRow row, uint64_t now_ns)
{
    (void)app;
    (void)row;
    (void)now_ns;
    return false;
}

void appNetworkUpdate(App *app, uint64_t now_ns)
{
    (void)app;
    (void)now_ns;
}

void appNetworkFeedSettings(void)
{
    if (g_preview_net_configured)
        settingsSetNetwork("\\\\NAS\\Books", "reader", true, false, true);
    else
        settingsSetNetwork("", "", true, false, false);
}

bool appNetworkEnsurePassword(App *app, uint64_t now_ns)
{
    (void)app;
    (void)now_ns;
    return true;
}

// --- The book server ---

bool opdsConfigured(void)
{
    return g_preview_server_configured;
}

void opdsGetAccount(DavAccount *out)
{
    memset(out, 0, sizeof(*out));
    if (g_preview_server_configured)
        snprintf(out->address, sizeof(out->address), "%s", "http://nas:8083/opds");
}

void opdsUseAccount(const DavAccount *a)
{
    (void)a;
}

void opdsLoadAccount(const char *path)
{
    (void)path;
}

bool opdsSaveAccount(const char *path)
{
    (void)path;
    return true;
}

bool opdsFindCatalog(const char *address, char *catalog, size_t size, char *why, size_t why_size)
{
    snprintf(catalog, size, "%s", address);
    if (why_size)
        why[0] = '\0';
    return true;
}

bool opdsTest(char *out, size_t size)
{
    snprintf(out, size, "%s", "The server answers.");
    return true;
}

bool opdsLinkOf(const char *path, char *out, size_t size)
{
    snprintf(out, size, "%s", path + strlen(OPDS_PREFIX));
    return true;
}

bool opdsLocalPath(const char *books_folder, const char *path, const char *title, char *out, size_t size)
{
    (void)path;
    snprintf(out, size, "%s/.opds/%s.epub", books_folder, title);
    return true;
}

void opdsRememberCover(const char *path, const char *url)
{
    (void)path;
    (void)url;
}

void opdsForgetCovers(void)
{
}

// A served book's cover: its fixture's own (as a server's thumbnail would show).
bool opdsFetchCover(const char *path, int w, int h, DocImage *out)
{
    const char *slash = strrchr(path, '/'), *hash = strrchr(path, '#');
    char file[256];
    snprintf(file, sizeof(file), "books/%.*s", (int)(hash && slash && hash > slash ? hash - slash - 1 : 64),
             slash ? slash + 1 : path);
    return docEpubCover(file, w, h, out);
}

bool netJobServerScan(const char *catalog)
{
    (void)catalog;
    g_server_ready = true;
    return true;
}

bool netJobServerScanning(int *found)
{
    *found = 0;
    return false;
}

bool netJobTakeServerScan(OpdsBook **books, int *count, bool *ok, char *why, size_t why_size)
{
    if (!g_server_ready)
        return false;
    g_server_ready = false;
    int n = (int)(sizeof(SERVED) / sizeof(SERVED[0]));
    *books = calloc((size_t)n, sizeof(**books));
    *count = 0;
    for (int i = 0; *books && i < n; i++) {
        OpdsBook *b = &(*books)[(*count)++];
        snprintf(b->id, sizeof(b->id), "urn:%d", i);
        snprintf(b->title, sizeof(b->title), "%s", SERVED[i].title);
        snprintf(b->author, sizeof(b->author), "%s", SERVED[i].author);
        snprintf(b->genre, sizeof(b->genre), "%s", SERVED[i].genre);
        snprintf(b->acquire, sizeof(b->acquire), "http://nas:8083/opds/download/%s", SERVED[i].file);
        const char *dot = strrchr(SERVED[i].file, '.');
        snprintf(b->ext, sizeof(b->ext), "%s", dot ? dot : ".epub");
        snprintf(b->cover, sizeof(b->cover), "http://nas:8083/opds/cover/%d", i);
        b->size = -1;
    }
    *ok = true;
    if (why_size)
        why[0] = '\0';
    return true;
}

bool netJobCopyLink(const char *link, const char *local, int64_t size)
{
    return netJobCopy(link, local, size);
}

SystemKeyboardResult systemKeyboardInput(const char *title, const char *initial, char *out, size_t size,
                                         size_t max_length)
{
    (void)title;
    (void)initial;
    (void)out;
    (void)size;
    (void)max_length;
    return SYSTEM_KEYBOARD_CANCELLED;
}

SystemKeyboardResult systemKeyboardPassword(const char *title, char *out, size_t size, size_t max_length)
{
    (void)title;
    (void)out;
    (void)size;
    (void)max_length;
    return SYSTEM_KEYBOARD_CANCELLED;
}

void appNetworkUse(void)
{
}
