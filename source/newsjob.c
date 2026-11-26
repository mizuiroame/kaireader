#include "newsjob.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "article.h"
#include "feedlist.h"
#include "lang.h"
#include "net.h"
#include "opds.h"

#define THREAD_STACK (256 * 1024)
#define FEED_MAX_BYTES (8u << 20)    // A feed's answer, at most.
#define KEPT_MAX_BYTES (12u << 20)   // A feed kept on the card, read back.
#define PAGE_MAX_BYTES (6u << 20)    // An article's page.
#define STALL_S 20
#define TIMEOUT_S 90

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool g_cancel;  // Every transfer stops (at exit).

static bool startThread(pthread_t *thread, void *(*run)(void *))
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, THREAD_STACK);
    bool ok = pthread_create(thread, &attr, run, NULL) == 0;
    pthread_attr_destroy(&attr);
    return ok;
}

// --- Asking the sites ---

// What went wrong, for the person (a page, when page is true; else a feed).
static void sayProblem(const NetResponse *r, bool page, char *why, size_t why_size)
{
    if (r->problem == NET_STATUS && opdsBusyStatus(r->status))
        snprintf(why, why_size, "%s", T("It isn't answering right now (it may be down for maintenance): try again later."));
    else if (r->problem == NET_CERTIFICATE)
        snprintf(why, why_size, "%s", T("The server's certificate isn't trusted."));
    else if (r->problem == NET_NO_CONNECT || r->problem == NET_NO_NAME || r->problem == NET_TIMEOUT)
        snprintf(why, why_size, "%s", T("The server doesn't answer: check its address, and that it's on."));
    else if (!page && r->problem == NET_STATUS && r->status == 404)
        snprintf(why, why_size, "%s", T("There's no feed at that address."));
    else
        snprintf(why, why_size, "%s", page ? T("The article's page couldn't be read.") : T("The feed couldn't be read."));
}

// A whole answer (unpacked if it came packed), and the address it came from after any redirect.
static bool fetch(const char *url, bool any_certificate, size_t max, NetResponse *r, char *final, size_t final_size)
{
    memset(r, 0, sizeof(*r));
    if (final_size)
        final[0] = '\0';
    NetClient *c = netClientOpen();
    if (!c) {
        r->problem = NET_OTHER;
        return false;
    }
    NetRequest q = {.url = url, .stall_s = STALL_S, .timeout_s = TIMEOUT_S, .any_certificate = any_certificate,
                    .compressed = true, .final_url = final, .final_url_size = final_size, .cancel = &g_cancel};
    bool ok = netFetchText(c, &q, max, r);
    netClientClose(c);
    if (final_size && !final[0])
        snprintf(final, final_size, "%s", url);
    return ok;
}

static char *readFile(const char *path, size_t max, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    char *data = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n + 65536 + 1 > cap) {
            size_t grown_cap = cap ? cap * 2 : 131072;
            char *grown = realloc(data, grown_cap);
            if (!grown)
                break;
            data = grown;
            cap = grown_cap;
        }
        size_t got = fread(data + n, 1, 65536, f);
        n += got;
        if (got < 65536 || n > max)
            break;
    }
    bool ok = !ferror(f) && data && n <= max;
    fclose(f);
    if (!ok) {
        free(data);
        return NULL;
    }
    data[n] = '\0';
    *size = n;
    return data;
}

static bool writeFile(const char *path, const char *data, size_t size)
{
    char tmp[1100];
    if (snprintf(tmp, sizeof(tmp), "%s.new", path) >= (int)sizeof(tmp))
        return false;
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp);
        return false;
    }
    remove(path);  // newlib's rename doesn't replace a file on the SD.
    return rename(tmp, path) == 0;
}

// --- Checking the feeds ---

static pthread_t g_check_thread;
static bool g_check_joinable, g_check_running, g_check_ready, g_check_asked, g_stop;
static NewsJobFeed *g_check_feeds;
static int g_check_count, g_check_done;
static char g_check_folder[1024];
static NewsArticle *g_articles;
static int g_article_count;
static NewsFeedState *g_states;

static void keptPath(const char *folder, const char *hash, char *out, size_t size)
{
    snprintf(out, size, "%s/%s.xml", folder, hash);
}

// The articles of a feed, added to *list.
static void addArticles(const Feed *f, int feed, const char *hash, NewsArticle **list, int *count, int *cap)
{
    for (int i = 0; i < f->count && *count < SHELF_MAX_BOOKS; i++) {
        if (*count == *cap) {
            int grown_cap = *cap ? *cap * 2 : 256;
            NewsArticle *grown = realloc(*list, (size_t)grown_cap * sizeof(*grown));
            if (!grown)
                return;
            *list = grown;
            *cap = grown_cap;
        }
        NewsArticle *a = &(*list)[*count];
        if (!newsPath(hash, f->items[i].id, a->path, sizeof(a->path)))
            continue;
        snprintf(a->title, sizeof(a->title), "%s", f->items[i].title);
        snprintf(a->image, sizeof(a->image), "%s", f->items[i].image);
        a->date = f->items[i].date;
        a->feed = feed;
        a->seq = *count;
        (*count)++;
    }
}

static int newestFirst(const void *x, const void *y)
{
    const NewsArticle *a = x, *b = y;
    if (a->date != b->date)
        return !a->date ? 1 : !b->date ? -1 : a->date > b->date ? -1 : 1;
    if (a->feed != b->feed)
        return a->feed < b->feed ? -1 : 1;
    return a->seq < b->seq ? -1 : a->seq > b->seq ? 1 : 0;
}

// One feed: what's kept of it, and (ask) what's new, merged and kept again.
static void checkFeed(const NewsJobFeed *nf, const char *folder, bool ask, Feed *out, NewsFeedState *state)
{
    char path[1100];
    keptPath(folder, nf->hash, path, sizeof(path));
    size_t size;
    char *data = readFile(path, KEPT_MAX_BYTES, &size);
    if (!data || !feedsParse(data, size, nf->url, out))
        memset(out, 0, sizeof(*out));
    free(data);
    state->ok = !ask;
    if (!ask)
        return;
    NetResponse r;
    char final[FEED_URL_MAX];
    if (!fetch(nf->url, nf->any_certificate, FEED_MAX_BYTES, &r, final, sizeof(final))) {
        sayProblem(&r, false, state->why, sizeof(state->why));
        netResponseFree(&r);
        return;
    }
    Feed fresh;
    bool parsed = feedsParse(r.data, r.size, final, &fresh);
    netResponseFree(&r);
    if (!parsed) {
        snprintf(state->why, sizeof(state->why), "%s", T("That address isn't a news feed, and its page doesn't link to one."));
        return;
    }
    if (!feedsMerge(out, &fresh, FEED_KEEP)) {
        feedsFree(&fresh);
        snprintf(state->why, sizeof(state->why), "%s", T("The feed couldn't be read."));
        return;
    }
    state->ok = true;
    size_t written;
    char *xml = feedsWrite(out, &written);
    if (xml)
        writeFile(path, xml, written);  // Not kept: it's asked again next time.
    free(xml);
}

// A round's articles and states handed over (replacing ones not taken).
static void publish(NewsArticle *list, int count, NewsFeedState *states, bool asked)
{
    if (count > 1)
        qsort(list, (size_t)count, sizeof(*list), newestFirst);
    pthread_mutex_lock(&g_lock);
    free(g_articles);
    free(g_states);
    g_articles = list;
    g_article_count = count;
    g_states = states;
    g_check_asked = asked;
    g_check_ready = true;
    pthread_mutex_unlock(&g_lock);
}

static void *checkThread(void *arg)
{
    (void)arg;
    for (int round = 0; round < 2; round++) {  // What's on the card first, then what the sites say.
        NewsArticle *list = NULL;
        int count = 0, cap = 0;
        NewsFeedState *states = calloc((size_t)(g_check_count > 0 ? g_check_count : 1), sizeof(*states));
        bool stopped = false;
        for (int i = 0; states && i < g_check_count; i++) {
            pthread_mutex_lock(&g_lock);
            stopped = g_stop;
            if (round == 1)
                g_check_done = i;
            pthread_mutex_unlock(&g_lock);
            if (stopped)
                break;
            Feed f;
            checkFeed(&g_check_feeds[i], g_check_folder, round == 1, &f, &states[i]);
            snprintf(states[i].title, sizeof(states[i].title), "%s", f.title);
            snprintf(states[i].hash, sizeof(states[i].hash), "%s", g_check_feeds[i].hash);
            addArticles(&f, i, g_check_feeds[i].hash, &list, &count, &cap);
            feedsFree(&f);
        }
        if (stopped || !states) {
            free(list);
            free(states);
            break;
        }
        publish(list, count, states, round == 1);
    }
    pthread_mutex_lock(&g_lock);
    g_check_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool newsJobCheck(const NewsJobFeed *feeds, int count, const char *folder)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_check_running;
    pthread_mutex_unlock(&g_lock);
    if (busy)
        return false;
    if (g_check_joinable)
        pthread_join(g_check_thread, NULL);
    g_check_joinable = false;
    free(g_check_feeds);
    g_check_feeds = count > 0 ? malloc((size_t)count * sizeof(*feeds)) : NULL;
    if (count > 0 && !g_check_feeds)
        return false;
    if (count > 0)
        memcpy(g_check_feeds, feeds, (size_t)count * sizeof(*feeds));
    g_check_count = count;
    g_check_done = 0;
    snprintf(g_check_folder, sizeof(g_check_folder), "%s", folder);
    pthread_mutex_lock(&g_lock);
    g_stop = false;
    g_check_running = true;
    pthread_mutex_unlock(&g_lock);
    g_check_joinable = startThread(&g_check_thread, checkThread);
    if (!g_check_joinable) {
        pthread_mutex_lock(&g_lock);
        g_check_running = false;
        pthread_mutex_unlock(&g_lock);
    }
    return g_check_joinable;
}

bool newsJobChecking(int *done, int *count)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_check_running;
    *done = g_check_done;
    *count = g_check_count;
    pthread_mutex_unlock(&g_lock);
    return busy;
}

bool newsJobTakeCheck(NewsArticle **articles, int *count, NewsFeedState **feeds, int *feed_count, bool *asked)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_check_ready;
    if (got) {
        *feed_count = g_check_count;
        *articles = g_articles;
        *count = g_article_count;
        *feeds = g_states;
        *asked = g_check_asked;
        g_articles = NULL;
        g_states = NULL;
        g_check_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

// --- Finding a feed ---

static pthread_t g_find_thread;
static bool g_find_joinable, g_find_running, g_find_ready, g_find_ok;
static char g_find_url[FEED_URL_MAX], g_find_title[FEED_TEXT_MAX], g_find_why[NETJOB_WHY_MAX];

// The feed at url (or linked from the page there): its address and title. False with why.
static bool findFeed(const char *url, char *found, size_t found_size, char *title, size_t title_size, char *why,
                     size_t why_size)
{
    char at[FEED_URL_MAX];
    snprintf(at, sizeof(at), "%s", url);
    for (int step = 0; step < 2; step++) {  // The address, then the feed its page links to.
        NetResponse r;
        char final[FEED_URL_MAX];
        if (!fetch(at, false, FEED_MAX_BYTES, &r, final, sizeof(final))) {
            sayProblem(&r, false, why, why_size);
            netResponseFree(&r);
            return false;
        }
        Feed f;
        if (feedsParse(r.data, r.size, final, &f)) {
            snprintf(found, found_size, "%s", final);
            snprintf(title, title_size, "%s", f.title);
            feedsFree(&f);
            netResponseFree(&r);
            return true;
        }
        char linked[FEED_URL_MAX];
        bool links = step == 0 && feedsDiscover(r.data, r.size, final, linked, sizeof(linked));
        netResponseFree(&r);
        if (!links)
            break;
        snprintf(at, sizeof(at), "%s", linked);
    }
    snprintf(why, why_size, "%s", T("That address isn't a news feed, and its page doesn't link to one."));
    return false;
}

static void *findThread(void *arg)
{
    (void)arg;
    char url[FEED_URL_MAX], found[FEED_URL_MAX] = "", title[FEED_TEXT_MAX] = "", why[NETJOB_WHY_MAX] = "";
    pthread_mutex_lock(&g_lock);
    snprintf(url, sizeof(url), "%s", g_find_url);
    pthread_mutex_unlock(&g_lock);
    bool ok = findFeed(url, found, sizeof(found), title, sizeof(title), why, sizeof(why));
    pthread_mutex_lock(&g_lock);
    snprintf(g_find_url, sizeof(g_find_url), "%s", ok ? found : url);
    snprintf(g_find_title, sizeof(g_find_title), "%s", title);
    snprintf(g_find_why, sizeof(g_find_why), "%s", why);
    g_find_ok = ok;
    g_find_ready = true;
    g_find_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool newsJobFind(const char *url)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_find_running;
    if (!busy) {
        g_find_running = true;
        g_find_ready = false;
        snprintf(g_find_url, sizeof(g_find_url), "%s", url);
    }
    pthread_mutex_unlock(&g_lock);
    if (busy)
        return false;
    if (g_find_joinable)
        pthread_join(g_find_thread, NULL);
    g_find_joinable = startThread(&g_find_thread, findThread);
    if (!g_find_joinable) {
        pthread_mutex_lock(&g_lock);
        g_find_running = false;
        pthread_mutex_unlock(&g_lock);
    }
    return g_find_joinable;
}

bool newsJobFinding(void)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_find_running;
    pthread_mutex_unlock(&g_lock);
    return busy;
}

bool newsJobTakeFind(char *url, size_t url_size, char *title, size_t title_size, bool *ok, char *why,
                     size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_find_ready;
    if (got) {
        snprintf(url, url_size, "%s", g_find_url);
        snprintf(title, title_size, "%s", g_find_title);
        snprintf(why, why_size, "%s", g_find_why);
        *ok = g_find_ok;
        g_find_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

// --- An article's whole page ---

static pthread_t g_full_thread;
static bool g_full_joinable, g_full_running, g_full_ready, g_full_ok, g_full_any;
static char g_full_link[FEED_URL_MAX], g_full_why[NETJOB_WHY_MAX];
static char *g_full_html;

static void *fullThread(void *arg)
{
    (void)arg;
    char link[FEED_URL_MAX], why[NETJOB_WHY_MAX] = "", final[FEED_URL_MAX];
    pthread_mutex_lock(&g_lock);
    snprintf(link, sizeof(link), "%s", g_full_link);
    bool any = g_full_any;
    pthread_mutex_unlock(&g_lock);
    NetResponse r;
    char *html = NULL;
    if (fetch(link, any, PAGE_MAX_BYTES, &r, final, sizeof(final))) {
        html = articleExtract(r.data, r.size);
        if (!html)
            snprintf(why, sizeof(why), "%s", T("No article could be found on its page."));
    } else {
        sayProblem(&r, true, why, sizeof(why));
    }
    netResponseFree(&r);
    pthread_mutex_lock(&g_lock);
    free(g_full_html);
    g_full_html = html;
    g_full_ok = html != NULL;
    snprintf(g_full_why, sizeof(g_full_why), "%s", why);
    g_full_ready = true;
    g_full_running = false;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool newsJobFull(const char *link, bool any_certificate)
{
    pthread_mutex_lock(&g_lock);
    bool busy = g_full_running;
    if (!busy) {
        g_full_running = true;
        g_full_ready = false;
        g_full_any = any_certificate;
        snprintf(g_full_link, sizeof(g_full_link), "%s", link);
    }
    pthread_mutex_unlock(&g_lock);
    if (busy)
        return false;
    if (g_full_joinable)
        pthread_join(g_full_thread, NULL);
    g_full_joinable = startThread(&g_full_thread, fullThread);
    if (!g_full_joinable) {
        pthread_mutex_lock(&g_lock);
        g_full_running = false;
        pthread_mutex_unlock(&g_lock);
    }
    return g_full_joinable;
}

bool newsJobTakeFull(char **html, bool *ok, char *why, size_t why_size)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_full_ready;
    if (got) {
        *html = g_full_html;
        *ok = g_full_ok;
        snprintf(why, why_size, "%s", g_full_why);
        g_full_html = NULL;
        g_full_ready = false;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

void newsJobStop(void)
{
    g_cancel = true;
    pthread_mutex_lock(&g_lock);
    g_stop = true;
    pthread_mutex_unlock(&g_lock);
    if (g_check_joinable)
        pthread_join(g_check_thread, NULL);
    if (g_find_joinable)
        pthread_join(g_find_thread, NULL);
    if (g_full_joinable)
        pthread_join(g_full_thread, NULL);
    g_check_joinable = g_find_joinable = g_full_joinable = false;
    free(g_check_feeds);
    g_check_feeds = NULL;
    free(g_articles);
    g_articles = NULL;
    free(g_states);
    g_states = NULL;
    free(g_full_html);
    g_full_html = NULL;
}
