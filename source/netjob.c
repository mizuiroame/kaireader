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
#include "catalogs.h"
#include "opdsnet.h"

#define COPY_CHUNK (1024 * 1024)
#define COPY_STALL_S 30
#define URL_MAX 2048
#define AUTH_MAX 512
#define THREAD_STACK (256 * 1024)

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static bool startThread(pthread_t *thread, void *(*run)(void *))
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, THREAD_STACK);
    bool ok = pthread_create(thread, &attr, run, NULL) == 0;
    pthread_attr_destroy(&attr);
    return ok;
}

// --- Looking through the whole folder ---

static pthread_t g_scan_thread;
static bool g_scan_joinable, g_scan_running, g_scan_ready, g_scan_ok, g_scan_stop;
static NetBook *g_scan_books;
static int g_scan_count;
static char g_scan_why[NETJOB_WHY_MAX];

typedef struct {
    char location[DAV_ADDRESS_MAX];
    int depth;
} Folder;

// One folder's books into books (grown), its folders onto the queue.
static bool scanFolder(const Folder *at, NetBook **books, int *count, int *cap, Folder **queue, int *queued,
                       int *queue_cap, char *why, size_t why_size)
{
    DavEntry *entries = NULL;
    size_t n = 0;
    if (!davList(at->location, &entries, &n, why, why_size))
        return false;
    n = netListBooks(entries, n);
    for (size_t i = 0; i < n && *count < NETJOB_SCAN_MAX; i++) {
        char child[DAV_ADDRESS_MAX];
        if (!netChild(at->location, entries[i].name, child, sizeof(child)))
            continue;
        if (entries[i].is_dir) {
            if (at->depth + 1 > NETJOB_SCAN_DEPTH)
                continue;
            if (*queued == *queue_cap) {
                Folder *grown = realloc(*queue, (size_t)(*queue_cap * 2 + 8) * sizeof(**queue));
                if (!grown)
                    continue;
                *queue = grown;
                *queue_cap = *queue_cap * 2 + 8;
            }
            Folder *f = &(*queue)[(*queued)++];
            snprintf(f->location, sizeof(f->location), "%s", child);
            f->depth = at->depth + 1;
            continue;
        }
        if (*count == *cap) {
            NetBook *grown = realloc(*books, (size_t)(*cap * 2 + 64) * sizeof(**books));
            if (!grown)
                break;
            *books = grown;
            *cap = *cap * 2 + 64;
        }
        NetBook *b = &(*books)[(*count)++];
        snprintf(b->location, sizeof(b->location), "%s", child);
        b->size = entries[i].size;
    }
    free(entries);
    return true;
}

static void *scanThread(void *arg)
{
    (void)arg;
    NetBook *books = NULL;
    Folder *queue = malloc(8 * sizeof(*queue));
    int count = 0, cap = 0, queued = 0, queue_cap = queue ? 8 : 0, done = 0;
    char why[NETJOB_WHY_MAX] = "";
    bool ok = queue != NULL;
    if (ok) {
        snprintf(queue[0].location, sizeof(queue[0].location), "%s", DAV_ROOT);
        queue[0].depth = 0;
        queued = 1;
    }
    while (ok && done < queued && count < NETJOB_SCAN_MAX) {
        pthread_mutex_lock(&g_lock);
        bool stop = g_scan_stop;
        g_scan_count = count;  // How many so far, for the screen.
        pthread_mutex_unlock(&g_lock);
        if (stop)
            break;
        Folder at = queue[done++];
        char folder_why[NETJOB_WHY_MAX];
        bool listed = scanFolder(&at, &books, &count, &cap, &queue, &queued, &queue_cap, folder_why, sizeof(folder_why));
        if (!listed && at.depth == 0) {  // The top can't be read: nothing found means nothing.
            ok = false;
            snprintf(why, sizeof(why), "%s", folder_why);
        }
    }
    free(queue);
    pthread_mutex_lock(&g_lock);
    free(g_scan_books);
    g_scan_books = books;
    g_scan_count = count;
    g_scan_ok = ok;
    snprintf(g_scan_why, sizeof(g_scan_why), "%s", why);
    g_scan_ready = true;
    g_scan_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool netJobScan(void)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_scan_running;
    pthread_mutex_unlock(&g_lock);
    if (busy)
        return false;
    if (g_scan_joinable)
        pthread_join(g_scan_thread, NULL);
    g_scan_joinable = false;
    pthread_mutex_lock(&g_lock);
    g_scan_running = true;
    g_scan_ready = false;
    g_scan_stop = false;
    g_scan_count = 0;
    pthread_mutex_unlock(&g_lock);
    if (startThread(&g_scan_thread, scanThread)) {
        g_scan_joinable = true;
        return true;
    }
    pthread_mutex_lock(&g_lock);
    g_scan_running = false;
    g_scan_ready = true;
    g_scan_ok = false;
    snprintf(g_scan_why, sizeof(g_scan_why), "%s", T("The folder couldn't be read."));
    pthread_mutex_unlock(&g_lock);
    return true;
}

bool netJobScanning(int *found)
{
    pthread_mutex_lock(&g_lock);
    bool running = g_scan_running;
    *found = g_scan_count;
    pthread_mutex_unlock(&g_lock);
    return running;
}

bool netJobTakeScan(NetBook **books, int *count, bool *ok, char *why, size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_scan_ready;
    if (got) {
        *books = g_scan_books;
        *count = g_scan_count;
        *ok = g_scan_ok;
        snprintf(why, why_size, "%s", g_scan_why);
        g_scan_books = NULL;
        g_scan_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

// --- Looking through a book server's catalog ---

static pthread_t g_server_thread;
static bool g_server_joinable, g_server_running, g_server_ready, g_server_ok, g_server_stop;
static OpdsBook *g_server_books;
static int g_server_count;
static char g_server_why[NETJOB_WHY_MAX];
static char g_server_catalog[OPDS_URL_MAX];

static bool seen(char (*urls)[OPDS_URL_MAX], int count, const char *url)
{
    for (int i = 0; i < count; i++)
        if (!strcmp(urls[i], url))
            return true;
    return false;
}

static bool knownBook(const OpdsBook *books, int count, const OpdsBook *b)
{
    for (int i = 0; i < count; i++)
        if (!strcmp(books[i].id, b->id) || !strcmp(books[i].acquire, b->acquire))
            return true;
    return false;
}

static void *serverThread(void *arg)
{
    (void)arg;
    char (*queue)[OPDS_URL_MAX] = malloc(NETJOB_SERVER_FEEDS * sizeof(*queue));
    OpdsBook *books = NULL;
    int queued = 0, done = 0, count = 0, cap = 0;
    char why[NETJOB_WHY_MAX] = "";
    bool ok = queue != NULL;
    if (ok) {
        char address[OPDS_URL_MAX];
        pthread_mutex_lock(&g_lock);
        snprintf(address, sizeof(address), "%s", g_server_catalog);
        pthread_mutex_unlock(&g_lock);
        // The address as typed may be the server's, not its catalog's (/opds...): found first.
        ok = opdsFindCatalog(address, queue[queued], OPDS_URL_MAX, why, sizeof(why));
        queued += ok;
    }
    while (ok && done < queued && count < NETJOB_SCAN_MAX) {
        pthread_mutex_lock(&g_lock);
        bool stop = g_server_stop;
        g_server_count = count;
        pthread_mutex_unlock(&g_lock);
        if (stop)
            break;
        OpdsFeed f;
        char feed_why[NETJOB_WHY_MAX];
        bool read = opdsFetchFeed(queue[done], &f, feed_why, sizeof(feed_why));
        if (!read) {
            if (done == 0) {  // The catalog itself can't be read: nothing found means nothing.
                ok = false;
                snprintf(why, sizeof(why), "%s", feed_why);
            }
            done++;
            continue;
        }
        done++;
        for (int i = 0; i < f.book_count && count < NETJOB_SCAN_MAX; i++) {
            if (knownBook(books, count, &f.books[i]))
                continue;
            if (count == cap) {
                OpdsBook *grown = realloc(books, (size_t)(cap * 2 + 64) * sizeof(*books));
                if (!grown)
                    break;
                books = grown;
                cap = cap * 2 + 64;
            }
            books[count++] = f.books[i];
        }
        // Its next page first (the rest of a list), then the lists it leads to.
        if (f.next[0] && queued < NETJOB_SERVER_FEEDS && !seen(queue, queued, f.next))
            snprintf(queue[queued++], OPDS_URL_MAX, "%s", f.next);
        for (int i = 0; i < f.feed_count && queued < NETJOB_SERVER_FEEDS; i++)
            if (!seen(queue, queued, f.feeds[i].url))
                snprintf(queue[queued++], OPDS_URL_MAX, "%s", f.feeds[i].url);
        opdsFeedFree(&f);
    }
    free(queue);
    pthread_mutex_lock(&g_lock);
    free(g_server_books);
    g_server_books = books;
    g_server_count = count;
    g_server_ok = ok;
    snprintf(g_server_why, sizeof(g_server_why), "%s", why);
    g_server_ready = true;
    g_server_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool netJobServerScan(const char *catalog)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_server_running;
    pthread_mutex_unlock(&g_lock);
    if (busy)
        return false;
    if (g_server_joinable)
        pthread_join(g_server_thread, NULL);
    g_server_joinable = false;
    pthread_mutex_lock(&g_lock);
    snprintf(g_server_catalog, sizeof(g_server_catalog), "%s", catalog);
    g_server_running = true;
    g_server_ready = false;
    g_server_stop = false;
    g_server_count = 0;
    pthread_mutex_unlock(&g_lock);
    if (startThread(&g_server_thread, serverThread)) {
        g_server_joinable = true;
        return true;
    }
    pthread_mutex_lock(&g_lock);
    g_server_running = false;
    g_server_ready = true;
    g_server_ok = false;
    snprintf(g_server_why, sizeof(g_server_why), "%s", T("The server couldn't be read."));
    pthread_mutex_unlock(&g_lock);
    return true;
}

bool netJobServerScanning(int *found)
{
    pthread_mutex_lock(&g_lock);
    bool running = g_server_running;
    *found = g_server_count;
    pthread_mutex_unlock(&g_lock);
    return running;
}

bool netJobTakeServerScan(OpdsBook **books, int *count, bool *ok, char *why, size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_server_ready;
    if (got) {
        *books = g_server_books;
        *count = g_server_count;
        *ok = g_server_ok;
        snprintf(why, why_size, "%s", g_server_why);
        g_server_books = NULL;
        g_server_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

// --- A free catalog's page ---

static pthread_t g_page_thread;
static bool g_page_joinable, g_page_running, g_page_ready, g_page_ok;
static char g_page_asked[OPDS_URL_MAX], g_page_url[OPDS_URL_MAX], g_page_next[OPDS_URL_MAX];
static char g_page_why[NETJOB_WHY_MAX];
static OpdsBook *g_page_books;
static int g_page_count;

static void *pageThread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_lock);
    while (g_page_asked[0]) {
        char url[OPDS_URL_MAX];
        snprintf(url, sizeof(url), "%s", g_page_asked);
        g_page_asked[0] = '\0';
        pthread_mutex_unlock(&g_lock);
        OpdsFeed f;
        char why[NETJOB_WHY_MAX] = "";
        OpdsBook *books = NULL;
        int count = 0;
        bool ok = opdsFetchFeed(url, &f, why, sizeof(why));
        if (ok) {
            count = catalogBooks(&f, &books);
        }
        pthread_mutex_lock(&g_lock);
        free(g_page_books);  // One not taken: the newer one replaces it.
        g_page_books = books;
        g_page_count = count;
        g_page_ok = ok;
        snprintf(g_page_url, sizeof(g_page_url), "%s", url);
        snprintf(g_page_next, sizeof(g_page_next), "%s", ok ? f.next : "");
        snprintf(g_page_why, sizeof(g_page_why), "%s", why);
        g_page_ready = true;
        if (ok)
            opdsFeedFree(&f);
    }
    g_page_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

void netJobCatalogPage(const char *url)
{
    pthread_mutex_lock(&g_lock);
    snprintf(g_page_asked, sizeof(g_page_asked), "%s", url);
    bool start = !g_page_running;
    if (start)
        g_page_running = true;
    pthread_mutex_unlock(&g_lock);
    if (!start)
        return;  // The one running takes it next.
    if (g_page_joinable)
        pthread_join(g_page_thread, NULL);
    g_page_joinable = startThread(&g_page_thread, pageThread);
    if (g_page_joinable)
        return;
    pthread_mutex_lock(&g_lock);
    g_page_running = false;
    g_page_asked[0] = '\0';
    g_page_ready = true;
    g_page_ok = false;
    snprintf(g_page_url, sizeof(g_page_url), "%s", url);
    snprintf(g_page_why, sizeof(g_page_why), "%s", T("The catalog couldn't be read."));
    pthread_mutex_unlock(&g_lock);
}

bool netJobCatalogLoading(void)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_page_running || g_page_asked[0];
    pthread_mutex_unlock(&g_lock);
    return busy;
}

bool netJobTakeCatalogPage(char *url, size_t url_size, OpdsBook **books, int *count, char *next, size_t next_size,
                           bool *ok, char *why, size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_page_ready && !g_page_asked[0];
    if (got) {
        snprintf(url, url_size, "%s", g_page_url);
        snprintf(next, next_size, "%s", g_page_next);
        snprintf(why, why_size, "%s", g_page_why);
        *books = g_page_books;
        *count = g_page_count;
        *ok = g_page_ok;
        g_page_books = NULL;
        g_page_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

// --- Copying a book ---

static pthread_t g_copy_thread;
static bool g_copy_joinable;
static volatile bool g_cancel;
static NetJobState g_copy_state;
static char g_copy_location[OPDS_URL_MAX], g_copy_local[DAV_ADDRESS_MAX + 64];
static bool g_copy_link;  // g_copy_location is a book server's link.
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

// One GET of url, with auth ("" for none), into out.
static bool copyByUrl(const char *url, char *auth, FILE *out, char *why, size_t why_size, bool any_certificate)
{
    NetClient *c = netClientOpen();
    if (!c) {
        snprintf(why, why_size, "%s", T("The network can't be used."));
        return false;
    }
    Sink sink = {out, 0, false};
    NetRequest r = {.url = url, .authorization = auth[0] ? auth : NULL, .cancel = &g_cancel, .stall_s = COPY_STALL_S,
                    .any_certificate = any_certificate};
    long status = 0;
    bool ok = netFetch(c, &r, toFile, &sink, &status, why, why_size) && !sink.failed;
    netClientClose(c);
    memset(auth, 0, strlen(auth));  // The sign-in, not kept in memory.
    if (sink.failed)
        snprintf(why, why_size, "%s", T("It couldn't be written to the card."));
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
    return copyByUrl(url, auth, out, why, why_size, false);
}

// A book server's book: its link, with the server's sign-in.
static bool copyByLink(const char *link, FILE *out, char *why, size_t why_size)
{
    char auth[AUTH_MAX];
    if (!opdsAuthorization(link, auth, sizeof(auth)))
        auth[0] = '\0';
    return copyByUrl(link, auth, out, why, why_size, opdsAnyCertificate(link));
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
    char location[OPDS_URL_MAX], local[DAV_ADDRESS_MAX + 64], part[DAV_ADDRESS_MAX + 72];
    pthread_mutex_lock(&g_lock);
    bool link = g_copy_link;
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
        ok = link             ? copyByLink(location, out, why, sizeof(why))
             : davReadsFiles() ? copyByFile(location, out, why, sizeof(why))
                               : copyByHttp(location, out, why, sizeof(why));
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

static bool startCopy(const char *location, bool link, const char *local, int64_t size);

bool netJobCopy(const char *location, const char *local, int64_t size)
{
    return startCopy(location, false, local, size);
}

bool netJobCopyLink(const char *link, const char *local, int64_t size)
{
    return startCopy(link, true, local, size);
}

static bool startCopy(const char *location, bool link, const char *local, int64_t size)
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
    g_copy_link = link;
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
    pthread_mutex_lock(&g_lock);
    g_scan_stop = true;
    g_server_stop = true;
    pthread_mutex_unlock(&g_lock);
    if (g_server_joinable)
        pthread_join(g_server_thread, NULL);
    g_server_joinable = false;
    free(g_server_books);
    g_server_books = NULL;
    pthread_mutex_lock(&g_lock);
    g_page_asked[0] = '\0';
    pthread_mutex_unlock(&g_lock);
    if (g_page_joinable)
        pthread_join(g_page_thread, NULL);
    g_page_joinable = false;
    free(g_page_books);
    g_page_books = NULL;
    if (g_scan_joinable)
        pthread_join(g_scan_thread, NULL);
    g_scan_joinable = false;
    free(g_scan_books);
    g_scan_books = NULL;
    if (g_copy_joinable)
        pthread_join(g_copy_thread, NULL);
    g_copy_joinable = false;
}
