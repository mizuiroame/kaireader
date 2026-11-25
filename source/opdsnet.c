#include "opdsnet.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "net.h"
#include "netlist.h"

#define FEED_MAX_BYTES (8u << 20)
#define COVER_MAX_BYTES (4u << 20)
#define FEED_STALL_S 15
#define FEED_TIMEOUT_S 60
#define AUTH_MAX 512
#define ACCOUNT_TEXT_MAX 2048

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static DavAccount g_account;

void opdsUseAccount(const DavAccount *a)
{
    pthread_mutex_lock(&g_lock);
    g_account = *a;
    pthread_mutex_unlock(&g_lock);
}

void opdsGetAccount(DavAccount *out)
{
    pthread_mutex_lock(&g_lock);
    *out = g_account;
    pthread_mutex_unlock(&g_lock);
}

bool opdsConfigured(void)
{
    pthread_mutex_lock(&g_lock);
    bool set = g_account.address[0] != '\0';
    pthread_mutex_unlock(&g_lock);
    return set;
}

void opdsLoadAccount(const char *path)
{
    DavAccount a;
    char text[ACCOUNT_TEXT_MAX] = "";
    FILE *f = fopen(path, "r");
    if (f) {
        size_t n = fread(text, 1, sizeof(text) - 1, f);
        text[n] = '\0';
        fclose(f);
    }
    bool older = davAccountRead(&a, text);
    opdsUseAccount(&a);
    memset(text, 0, sizeof(text));
    memset(&a, 0, sizeof(a));
    if (older)
        opdsSaveAccount(path);
}

bool opdsSaveAccount(const char *path)
{
    DavAccount a;
    char text[ACCOUNT_TEXT_MAX];
    opdsGetAccount(&a);
    bool ok = davAccountWrite(&a, text, sizeof(text));
    FILE *f = ok ? fopen(path, "w") : NULL;
    ok = f && fputs(text, f) >= 0;
    if (f)
        ok = fclose(f) == 0 && ok;
    memset(text, 0, sizeof(text));
    memset(&a, 0, sizeof(a));
    return ok;
}

bool opdsAuthorization(const char *url, char *out, size_t size)
{
    if (size)
        out[0] = '\0';
    DavAccount a;
    opdsGetAccount(&a);
    // Only to the server's own address: never to a cover or book elsewhere, nor to a free catalog.
    bool ok = !a.address[0] || !davSameServer(url, a.address) || davAuthorization(&a, out, size);
    memset(&a, 0, sizeof(a));
    return ok;
}

// What went wrong, for the person.
static void sayProblem(const NetResponse *r, char *why, size_t why_size)
{
    if (r->problem == NET_STATUS && (r->status == 401 || r->status == 403))
        snprintf(why, why_size, "%s", T("Wrong user name or password."));
    else if (r->problem == NET_STATUS && (r->status == 502 || r->status == 503 || r->status == 504))
        snprintf(why, why_size, "%s", T("It isn't answering right now (it may be down for maintenance): try again later."));
    else if (r->problem == NET_STATUS && r->status == 404)
        snprintf(why, why_size, "%s", T("There's no catalog at that address."));
    else if (r->problem == NET_CERTIFICATE)
        snprintf(why, why_size, "%s", T("The server's certificate isn't trusted."));
    else if (r->problem == NET_NO_CONNECT || r->problem == NET_NO_NAME || r->problem == NET_TIMEOUT)
        snprintf(why, why_size, "%s", T("The server doesn't answer: check its address, and that it's on."));
    else
        snprintf(why, why_size, "%s", r->error[0] ? r->error : T("The server couldn't be read."));
}

// A whole answer of the server (with its sign-in), up to max bytes.
static bool fetch(const char *url, size_t max, NetResponse *r)
{
    char auth[AUTH_MAX];
    if (!opdsAuthorization(url, auth, sizeof(auth)))
        auth[0] = '\0';
    NetClient *c = netClientOpen();
    memset(r, 0, sizeof(*r));
    if (!c) {
        r->problem = NET_OTHER;
        memset(auth, 0, sizeof(auth));
        return false;
    }
    NetRequest q = {.url = url, .authorization = auth[0] ? auth : NULL, .stall_s = FEED_STALL_S,
                    .timeout_s = FEED_TIMEOUT_S};
    bool ok = netFetchText(c, &q, max, r);
    netClientClose(c);
    memset(auth, 0, sizeof(auth));
    return ok;
}

bool opdsFetchFeed(const char *url, OpdsFeed *out, char *why, size_t why_size)
{
    memset(out, 0, sizeof(*out));
    NetResponse r;
    if (!fetch(url, FEED_MAX_BYTES, &r)) {
        sayProblem(&r, why, why_size);
        netResponseFree(&r);
        return false;
    }
    bool ok = opdsParse(r.data, r.size, url, out);
    netResponseFree(&r);
    if (!ok)
        snprintf(why, why_size, "%s", T("That address isn't an OPDS catalog."));
    return ok;
}

bool opdsFindCatalog(const char *address, char *catalog, size_t size, char *why, size_t why_size)
{
    // As typed first; then where Calibre-Web, Komga and Kavita (with its key in the address) keep theirs.
    static const char *const UNDER[] = {"", "opds", "opds/v1.2/catalog", "opds-catalog", "api/opds"};
    char first_why[256] = "";
    for (size_t i = 0; i < sizeof(UNDER) / sizeof(UNDER[0]); i++) {
        char url[OPDS_URL_MAX];
        size_t len = strlen(address);
        int n = snprintf(url, sizeof(url), "%s%s%s", address, len && address[len - 1] == '/' ? "" : "/", UNDER[i]);
        if (i == 0)
            n = snprintf(url, sizeof(url), "%s", address);
        if (n <= 0 || (size_t)n >= sizeof(url))
            continue;
        OpdsFeed f;
        char tried_why[256];
        if (opdsFetchFeed(url, &f, tried_why, sizeof(tried_why))) {
            opdsFeedFree(&f);
            snprintf(catalog, size, "%s", url);
            return true;
        }
        if (i == 0)
            snprintf(first_why, sizeof(first_why), "%s", tried_why);
        if (!strcmp(tried_why, T("Wrong user name or password.")) ||
            !strcmp(tried_why, T("The server doesn't answer: check its address, and that it's on.")))
            break;  // The others won't do better.
    }
    snprintf(why, why_size, "%s", first_why);
    return false;
}

bool opdsTest(char *out, size_t size)
{
    DavAccount a;
    opdsGetAccount(&a);
    char address[DAV_ADDRESS_MAX];
    snprintf(address, sizeof(address), "%s", a.address);
    memset(&a, 0, sizeof(a));
    if (!address[0]) {
        snprintf(out, size, "%s", T("Enter the server's address first."));
        return false;
    }
    OpdsFeed f;
    char why[256];
    if (!opdsFetchFeed(address, &f, why, sizeof(why))) {
        snprintf(out, size, "%s", why);
        return false;
    }
    snprintf(out, size, T("The server answers: %s, with %d books and %d lists at its start."),
             f.title[0] ? f.title : T("its catalog"), f.book_count, f.feed_count);
    opdsFeedFree(&f);
    return true;
}

bool opdsLinkOf(const char *path, char *out, size_t size)
{
    size_t prefix = strlen(OPDS_PREFIX);
    if (strncmp(path, OPDS_PREFIX, prefix) != 0)
        return false;
    const char *hash = strrchr(path, '#');
    size_t len = hash ? (size_t)(hash - path) - prefix : strlen(path) - prefix;
    if (len + 1 > size)
        return false;
    memcpy(out, path + prefix, len);
    out[len] = '\0';
    return true;
}

bool opdsLocalPath(const char *books_folder, const char *path, const char *title, char *out, size_t size)
{
    const char *hash = strrchr(path, '#');
    const char *ext = hash && hash[1] == '.' ? hash + 1 : ".epub";
    char name[256], plain[256];
    // The title (or the link's last part) as a plain name; the link's hash keeps two of a title apart.
    unsigned h = 2166136261u;
    for (const char *p = path; *p; p++)
        h = (h ^ (unsigned char)*p) * 16777619u;
    const char *slash = strrchr(path, '/');
    snprintf(name, sizeof(name), "%s", title && title[0] ? title : slash ? slash + 1 : "book");
    netPlainName(name, true, plain, sizeof(plain));
    int n = snprintf(out, size, "%s/.opds/%.120s %06x%s", books_folder, plain, h & 0xFFFFFF, ext);
    return n > 0 && (size_t)n < size;
}

// --- Thumbnails ---

typedef struct {
    char *path, *url;
} CoverLink;

static CoverLink *g_covers;
static int g_cover_count, g_cover_cap;

void opdsRememberCover(const char *path, const char *url)
{
    if (!url || !url[0])
        return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_cover_count; i++)
        if (!strcmp(g_covers[i].path, path)) {
            char *copy = strdup(url);
            if (copy) {
                free(g_covers[i].url);
                g_covers[i].url = copy;
            }
            pthread_mutex_unlock(&g_lock);
            return;
        }
    if (g_cover_count == g_cover_cap) {
        int cap = g_cover_cap ? g_cover_cap * 2 : 64;
        CoverLink *grown = realloc(g_covers, (size_t)cap * sizeof(*grown));
        if (!grown) {
            pthread_mutex_unlock(&g_lock);
            return;
        }
        g_covers = grown;
        g_cover_cap = cap;
    }
    CoverLink l = {strdup(path), strdup(url)};
    if (l.path && l.url)
        g_covers[g_cover_count++] = l;
    else {
        free(l.path);
        free(l.url);
    }
    pthread_mutex_unlock(&g_lock);
}

void opdsForgetCovers(void)
{
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_cover_count; i++) {
        free(g_covers[i].path);
        free(g_covers[i].url);
    }
    free(g_covers);
    g_covers = NULL;
    g_cover_count = g_cover_cap = 0;
    pthread_mutex_unlock(&g_lock);
}

bool opdsFetchCover(const char *path, int w, int h, DocImage *out)
{
    char url[OPDS_URL_MAX] = "";
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_cover_count; i++)
        if (!strcmp(g_covers[i].path, path)) {
            snprintf(url, sizeof(url), "%s", g_covers[i].url);
            break;
        }
    pthread_mutex_unlock(&g_lock);
    if (!url[0])
        return false;
    NetResponse r;
    bool ok = fetch(url, COVER_MAX_BYTES, &r) && docImageFromData(r.data, r.size, w, h, out);
    netResponseFree(&r);
    return ok;
}
