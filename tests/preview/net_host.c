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
#include "netjob.h"

bool g_preview_net_configured;

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
