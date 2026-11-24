// The network for the previews: a made-up folder ("smb://NAS/Books/") whose
// listings come at once, a copy that stays at 40%, and the account's
// Settings rows filled in. Stands in for netjob.c, davnet.c and
// app_network.c, which need the console's network.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "davnet.h"
#include "netjob.h"
#include "netlist.h"

bool g_preview_net_configured;

static const DavEntry TOP[] = {
    {"Comics", true, -1, 0},
    {"Ensayos", true, -1, 0},
    {"Novels", true, -1, 0},
    {".Trash", true, -1, 0},
    {"The Harbor Ledger.pdf", false, 2283714, 0},
    {"A Map of Small Islands.epub", false, 512300, 0},
    {"Notes from the Lighthouse.epub", false, 1203400, 0},
    {"Caf\xC3\xA9 Stories.fb2", false, 301200, 0},
    {"Sample Comic.cbr", false, 4210000, 0},
    {"Tide Tables 2026.cbz", false, 9120000, 0},
    {"setup.exe", false, 102400, 0},
};

static char g_listed[DAV_ADDRESS_MAX];
static bool g_list_ready;
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

void netJobList(const char *location)
{
    snprintf(g_listed, sizeof(g_listed), "%s", location);
    g_list_ready = true;
}

bool netJobListing(void)
{
    return false;
}

bool netJobTakeList(char *location, size_t location_size, DavEntry **entries, size_t *count, bool *ok, char *why,
                    size_t why_size)
{
    if (!g_list_ready)
        return false;
    g_list_ready = false;
    size_t n = sizeof(TOP) / sizeof(TOP[0]);
    *entries = malloc(sizeof(TOP));
    memcpy(*entries, TOP, sizeof(TOP));
    *count = netListBooks(*entries, n);
    *ok = true;
    snprintf(location, location_size, "%s", g_listed);
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
