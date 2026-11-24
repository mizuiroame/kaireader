#include "netjob.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "approot.h"
#include "davnet.h"
#include "lang.h"
#include "net.h"
#include "netlist.h"

#define COPY_CHUNK (1024 * 1024)
#define COPY_STALL_S 30
#define URL_MAX 2048
#define AUTH_MAX 512
#define THREAD_STACK (256 * 1024)

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

// --- Listing ---

static pthread_t g_list_thread;
static bool g_list_running, g_list_joinable;
static char g_list_asked[DAV_ADDRESS_MAX];  // The next to list; "" none.
static bool g_list_ready, g_list_ok;
static char g_list_location[DAV_ADDRESS_MAX];
static char g_list_why[NETJOB_WHY_MAX];
static DavEntry *g_list_entries;
static size_t g_list_count;

static void *listThread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_lock);
    while (g_list_asked[0]) {
        char location[DAV_ADDRESS_MAX];
        snprintf(location, sizeof(location), "%s", g_list_asked);
        g_list_asked[0] = '\0';
        pthread_mutex_unlock(&g_lock);
        DavEntry *entries = NULL;
        size_t count = 0;
        char why[NETJOB_WHY_MAX] = "";
        bool ok = davList(location, &entries, &count, why, sizeof(why));
        if (ok)
            count = netListBooks(entries, count);
        pthread_mutex_lock(&g_lock);
        free(g_list_entries);  // One not taken: a newer one replaces it.
        g_list_entries = ok ? entries : NULL;
        if (!ok)
            free(entries);
        g_list_count = ok ? count : 0;
        g_list_ok = ok;
        snprintf(g_list_location, sizeof(g_list_location), "%s", location);
        snprintf(g_list_why, sizeof(g_list_why), "%s", why);
        g_list_ready = true;
    }
    g_list_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

static bool startThread(pthread_t *thread, void *(*run)(void *))
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, THREAD_STACK);
    bool ok = pthread_create(thread, &attr, run, NULL) == 0;
    pthread_attr_destroy(&attr);
    return ok;
}

void netJobList(const char *location)
{
    pthread_mutex_lock(&g_lock);
    snprintf(g_list_asked, sizeof(g_list_asked), "%s", location);
    bool start = !g_list_running;
    pthread_mutex_unlock(&g_lock);
    if (!start)
        return;  // The one running takes it next.
    if (g_list_joinable)
        pthread_join(g_list_thread, NULL);
    g_list_joinable = false;
    pthread_mutex_lock(&g_lock);
    g_list_running = true;
    pthread_mutex_unlock(&g_lock);
    if (startThread(&g_list_thread, listThread)) {
        g_list_joinable = true;
        return;
    }
    pthread_mutex_lock(&g_lock);  // No thread: it fails at once.
    g_list_running = false;
    g_list_asked[0] = '\0';
    g_list_ready = true;
    g_list_ok = false;
    snprintf(g_list_location, sizeof(g_list_location), "%s", location);
    snprintf(g_list_why, sizeof(g_list_why), "%s", T("The folder couldn't be read."));
    pthread_mutex_unlock(&g_lock);
}

bool netJobListing(void)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_list_running || g_list_asked[0];
    pthread_mutex_unlock(&g_lock);
    return busy;
}

bool netJobTakeList(char *location, size_t location_size, DavEntry **entries, size_t *count, bool *ok, char *why,
                    size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_list_ready && !g_list_asked[0];  // One asked since: wait for it.
    if (got) {
        snprintf(location, location_size, "%s", g_list_location);
        snprintf(why, why_size, "%s", g_list_why);
        *entries = g_list_entries;
        *count = g_list_count;
        *ok = g_list_ok;
        g_list_entries = NULL;
        g_list_count = 0;
        g_list_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

// --- Copying a book ---

static pthread_t g_copy_thread;
static bool g_copy_joinable;
static volatile bool g_cancel;
static NetJobState g_copy_state;
static char g_copy_location[DAV_ADDRESS_MAX], g_copy_local[DAV_ADDRESS_MAX + 64];
static int64_t g_copy_done, g_copy_total;
static char g_copy_why[NETJOB_WHY_MAX];

static void progress(int64_t done, int64_t total)
{
    pthread_mutex_lock(&g_lock);
    g_copy_done = done;
    if (total > 0)
        g_copy_total = total;
    pthread_mutex_unlock(&g_lock);
}

typedef struct {
    FILE *out;
    int64_t done;
    bool failed;
} Sink;

static bool toFile(const void *data, size_t size, void *user)
{
    Sink *s = user;
    if (fwrite(data, 1, size, s->out) != size) {
        s->failed = true;
        return false;
    }
    s->done += (int64_t)size;
    progress(s->done, -1);
    return !g_cancel;
}

// SMB, FTP or SFTP: the protocol's own reads.
static bool copyByFile(const char *location, FILE *out, char *why, size_t why_size)
{
    DavFile *f = davFileOpen(location, why, why_size);
    if (!f)
        return false;
    uint8_t *buffer = malloc(COPY_CHUNK);
    int64_t total = davFileSize(f), offset = 0;
    progress(0, total);
    bool ok = buffer != NULL;
    while (ok && !g_cancel) {
        int64_t n = davFileRead(f, offset, buffer, COPY_CHUNK, &g_cancel);
        if (n < 0) {
            ok = false;
            if (!g_cancel)
                snprintf(why, why_size, "%s", T("The book couldn't be read from the network folder."));
        } else if (n == 0) {
            break;
        } else if (fwrite(buffer, 1, (size_t)n, out) != (size_t)n) {
            ok = false;
            snprintf(why, why_size, "%s", T("It couldn't be written to the card."));
        } else {
            offset += n;
            progress(offset, -1);
        }
    }
    free(buffer);
    davFileClose(f);
    return ok && !g_cancel;
}

// WebDAV or a web server's folder: one GET.
static bool copyByHttp(const char *location, FILE *out, char *why, size_t why_size)
{
    char url[URL_MAX], auth[AUTH_MAX];
    if (!davStreamRequest(location, url, sizeof(url), auth, sizeof(auth))) {
        snprintf(why, why_size, "%s", T("Set up the network folder in Settings first."));
        return false;
    }
    NetClient *c = netClientOpen();
    if (!c) {
        snprintf(why, why_size, "%s", T("The network can't be used."));
        return false;
    }
    Sink sink = {out, 0, false};
    NetRequest r = {.url = url, .authorization = auth, .cancel = &g_cancel, .stall_s = COPY_STALL_S};
    long status = 0;
    bool ok = netFetch(c, &r, toFile, &sink, &status, why, why_size) && !sink.failed;
    netClientClose(c);
    memset(auth, 0, sizeof(auth));
    if (sink.failed)
        snprintf(why, why_size, "%s", T("It couldn't be written to the card."));
    return ok && !g_cancel;
}

// The local file's folder made.
static bool makeParent(const char *local)
{
    char folder[DAV_ADDRESS_MAX + 64];
    snprintf(folder, sizeof(folder), "%s", local);
    char *slash = strrchr(folder, '/');
    if (!slash)
        return false;
    *slash = '\0';
    return appRootMakeFolder(folder);
}

static void *copyThread(void *arg)
{
    (void)arg;
    char location[DAV_ADDRESS_MAX], local[DAV_ADDRESS_MAX + 64], part[DAV_ADDRESS_MAX + 72];
    pthread_mutex_lock(&g_lock);
    snprintf(location, sizeof(location), "%s", g_copy_location);
    snprintf(local, sizeof(local), "%s", g_copy_local);
    pthread_mutex_unlock(&g_lock);
    snprintf(part, sizeof(part), "%s.part", local);
    char why[NETJOB_WHY_MAX] = "";
    bool ok = false;
    FILE *out = makeParent(local) ? fopen(part, "wb") : NULL;
    if (!out) {
        snprintf(why, sizeof(why), "%s", T("It couldn't be written to the card."));
    } else {
        ok = davReadsFiles() ? copyByFile(location, out, why, sizeof(why)) : copyByHttp(location, out, why, sizeof(why));
        if (fclose(out) != 0 && ok) {
            ok = false;
            snprintf(why, sizeof(why), "%s", T("It couldn't be written to the card."));
        }
        if (ok) {
            remove(local);  // An older copy.
            ok = rename(part, local) == 0;
            if (!ok)
                snprintf(why, sizeof(why), "%s", T("It couldn't be written to the card."));
        }
        if (!ok)
            remove(part);
    }
    pthread_mutex_lock(&g_lock);
    g_copy_state = ok ? NETJOB_DONE : NETJOB_FAILED;
    snprintf(g_copy_why, sizeof(g_copy_why), "%s", g_cancel ? "" : why);
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool netJobCopy(const char *location, const char *local, int64_t size)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_copy_state == NETJOB_BUSY;
    pthread_mutex_unlock(&g_lock);
    if (busy)
        return false;
    if (g_copy_joinable)
        pthread_join(g_copy_thread, NULL);
    g_copy_joinable = false;
    pthread_mutex_lock(&g_lock);
    snprintf(g_copy_location, sizeof(g_copy_location), "%s", location);
    snprintf(g_copy_local, sizeof(g_copy_local), "%s", local);
    g_copy_done = 0;
    g_copy_total = size;
    g_copy_why[0] = '\0';
    g_copy_state = NETJOB_BUSY;
    g_cancel = false;
    pthread_mutex_unlock(&g_lock);
    if (startThread(&g_copy_thread, copyThread)) {
        g_copy_joinable = true;
        return true;
    }
    pthread_mutex_lock(&g_lock);
    g_copy_state = NETJOB_FAILED;
    snprintf(g_copy_why, sizeof(g_copy_why), "%s", T("The book couldn't be read from the network folder."));
    pthread_mutex_unlock(&g_lock);
    return true;
}

void netJobCancelCopy(void)
{
    g_cancel = true;
}

NetJobState netJobCopyState(int64_t *done, int64_t *total, char *why, size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    NetJobState state = g_copy_state;
    *done = g_copy_done;
    *total = g_copy_total;
    snprintf(why, why_size, "%s", g_copy_why);
    pthread_mutex_unlock(&g_lock);
    return state;
}

void netJobForgetCopy(void)
{
    pthread_mutex_lock(&g_lock);
    if (g_copy_state != NETJOB_BUSY)
        g_copy_state = NETJOB_IDLE;
    pthread_mutex_unlock(&g_lock);
}

void netJobStop(void)
{
    g_cancel = true;
    if (g_copy_joinable)
        pthread_join(g_copy_thread, NULL);
    g_copy_joinable = false;
    pthread_mutex_lock(&g_lock);
    g_list_asked[0] = '\0';
    pthread_mutex_unlock(&g_lock);
    if (g_list_joinable)
        pthread_join(g_list_thread, NULL);
    g_list_joinable = false;
    pthread_mutex_lock(&g_lock);
    free(g_list_entries);
    g_list_entries = NULL;
    g_list_ready = false;
    pthread_mutex_unlock(&g_lock);
}
