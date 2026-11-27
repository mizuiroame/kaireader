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
#include "lang.h"
#include "netjob.h"
#include "opdsnet.h"

bool g_preview_net_configured;
bool g_preview_copy_unknown;
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
    *total = g_preview_copy_unknown ? -1 : 4210000;  // A server that doesn't say its size: -1.
    *done = 4210000 * 2 / 5;
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
        settingsSetNetwork("\\\\NAS\\Books", "reader", true, false, true, true);
    else
        settingsSetNetwork("", "", true, false, false, true);
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
    if (!strncmp(path, "news:", 5)) {  // A made-up article: a made-up picture for some, none for the rest.
        bool previewNewsCover(const char *path, int w, int h, DocImage *out);
        return previewNewsCover(path, w, h, out);
    }
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

// --- Free books: a made-up first page (its covers the fixtures') ---

static bool g_page_ready;
static char g_page_url[1024];
bool g_preview_free_down;  // The catalog down for maintenance (Project Gutenberg's 503).

void netJobCatalogPage(const char *url)
{
    snprintf(g_page_url, sizeof(g_page_url), "%s", url);
    g_page_ready = true;
}

bool netJobCatalogLoading(void)
{
    return false;
}

bool netJobTakeCatalogPage(char *url, size_t url_size, OpdsBook **books, int *count, char *next, size_t next_size,
                           bool *ok, char *why, size_t why_size)
{
    static const struct {
        const char *file, *title, *author;
    } PAGE[] = {
        {"library1.epub", "The Salt Ledger", "Mara Fennick"},  {"library2.epub", "North of the Bell", "Ivo Castellan"},
        {"library4.epub", "A Map of Small Islands", "Lio Arden"}, {"library5.epub", "The Gull Keeper", "Ivo Castellan"},
        {"library7.epub", "Fog Stair", "Lio Arden"},           {"old.epub", "An Older Harbor", "Ana Example"},
        {"tale.fb2", "A Tale of Ropes", "Cy Placeholder"},
    };
    if (!g_page_ready)
        return false;
    g_page_ready = false;
    if (g_preview_free_down) {
        snprintf(url, url_size, "%s", g_page_url);
        *books = NULL;
        *count = 0;
        if (next_size)
            next[0] = 0;
        *ok = false;
        snprintf(why, why_size, "%s",
                 T("It isn't answering right now (it may be down for maintenance): try again later."));
        return true;
    }
    int n = (int)(sizeof(PAGE) / sizeof(PAGE[0]));
    *books = calloc((size_t)n, sizeof(**books));
    *count = 0;
    for (int i = 0; *books && i < n; i++) {
        OpdsBook *b = &(*books)[(*count)++];
        snprintf(b->title, sizeof(b->title), "%s", PAGE[i].title);
        snprintf(b->author, sizeof(b->author), "%s", PAGE[i].author);
        snprintf(b->acquire, sizeof(b->acquire), "https://free.example/books/%s", PAGE[i].file);
        snprintf(b->ext, sizeof(b->ext), "%s", ".epub");
        b->size = -1;
    }
    snprintf(url, url_size, "%s", g_page_url);
    if (next_size)
        next[0] = 0;
    *ok = true;
    if (why_size)
        why[0] = 0;
    return true;
}

void appNetworkWantFree(void)
{
}

// The update check: nothing asked in the preview.
#include "updatefetch.h"
void updateFetchInit(const char *nro_path) { (void)nro_path; }
bool updateFetchCheck(void) { return false; }
bool updateFetchInstall(void) { return false; }
void updateFetchStatus(UpdateStatus *out) { memset(out, 0, sizeof(*out)); }
void updateFetchExit(void) {}

void opdsCheckCertificate(bool check) { (void)check; }

// --- The news: the made-up feeds preview.c keeps in the folder, read as newsjob.c reads them ---

#include "feedlist.h"
#include "feeds.h"
#include "newsjob.h"

bool g_preview_news_down;      // The sites don't answer: the articles kept, and why.
bool g_preview_news_checking;  // A check under way.
static NewsJobFeed g_news_feeds[FEEDLIST_MAX];
static int g_news_count;
static char g_news_folder[512];
static bool g_news_ready, g_find_ready, g_full_ready;
static char g_find_url[FEED_URL_MAX];

bool newsJobCheck(const NewsJobFeed *feeds, int count, const char *folder)
{
    g_news_count = count < FEEDLIST_MAX ? count : FEEDLIST_MAX;
    memcpy(g_news_feeds, feeds, (size_t)g_news_count * sizeof(*feeds));
    snprintf(g_news_folder, sizeof(g_news_folder), "%s", folder);
    g_news_ready = true;
    return true;
}

bool newsJobChecking(int *done, int *count)
{
    *done = 1;
    *count = g_news_count;
    return g_preview_news_checking;
}

static int newest(const void *x, const void *y)
{
    const NewsArticle *a = x, *b = y;
    return a->date != b->date ? (a->date > b->date ? -1 : 1) : a->seq - b->seq;
}

bool newsJobTakeCheck(NewsArticle **articles, int *count, NewsFeedState **feeds, int *feed_count, bool *asked)
{
    if (!g_news_ready || g_preview_news_checking)
        return false;
    g_news_ready = false;
    *articles = calloc(SHELF_MAX_BOOKS, sizeof(**articles));
    *feeds = calloc((size_t)(g_news_count ? g_news_count : 1), sizeof(**feeds));
    *count = 0;
    *feed_count = g_news_count;
    *asked = true;
    for (int i = 0; *articles && *feeds && i < g_news_count; i++) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s.xml", g_news_folder, g_news_feeds[i].hash);
        FILE *f = fopen(path, "rb");
        static char data[1 << 20];
        size_t n = f ? fread(data, 1, sizeof(data) - 1, f) : 0;
        if (f)
            fclose(f);
        Feed feed;
        (*feeds)[i].ok = !g_preview_news_down;
        snprintf((*feeds)[i].hash, sizeof((*feeds)[i].hash), "%s", g_news_feeds[i].hash);
        if (g_preview_news_down)
            snprintf((*feeds)[i].why, sizeof((*feeds)[i].why), "%s",
                     T("The server doesn't answer: check its address, and that it's on."));
        if (!n || !feedsParse(data, n, g_news_feeds[i].url, &feed))
            continue;
        snprintf((*feeds)[i].title, sizeof((*feeds)[i].title), "%s", feed.title);
        for (int j = 0; j < feed.count; j++) {
            NewsArticle *a = &(*articles)[*count];
            newsPath(g_news_feeds[i].hash, feed.items[j].id, a->path, sizeof(a->path));
            snprintf(a->title, sizeof(a->title), "%s", feed.items[j].title);
            snprintf(a->image, sizeof(a->image), "%s", feed.items[j].image);
            feedsPlainText(feed.items[j].html, a->summary, sizeof(a->summary));
            a->date = feed.items[j].date;
            a->feed = i;
            a->seq = (*count)++;
        }
        feedsFree(&feed);
    }
    if (*articles)
        qsort(*articles, (size_t)*count, sizeof(**articles), newest);
    return true;
}

bool newsJobFind(const char *url)
{
    snprintf(g_find_url, sizeof(g_find_url), "%s", url);
    g_find_ready = true;
    return true;
}

bool newsJobFinding(void)
{
    return false;
}

bool newsJobTakeFind(char *url, size_t url_size, char *title, size_t title_size, bool *ok, char *why,
                     size_t why_size)
{
    if (!g_find_ready)
        return false;
    g_find_ready = false;
    snprintf(url, url_size, "%s", g_find_url);
    snprintf(title, title_size, "%s", "Bay Log");
    *ok = true;
    if (why_size)
        why[0] = '\0';
    return true;
}

bool newsJobFull(const char *link, bool any_certificate)
{
    (void)link;
    (void)any_certificate;
    g_full_ready = true;
    return true;
}

bool newsJobTakeFull(char **html, bool *ok, char *why, size_t why_size)
{
    if (!g_full_ready)
        return false;
    g_full_ready = false;
    *html = strdup("<div><p>The whole made-up article, as its site would show it.</p></div>");
    *ok = *html != NULL;
    if (why_size)
        why[0] = '\0';
    return true;
}

void newsJobStop(void)
{
}

void appNetworkWantNews(void)
{
}

// An article's pictures: the made-up dusk picture of the previews (make_background.py), kept as
// newsjob.c keeps them (fitted, as JPEG files).
static bool g_pictures_ready;
static int g_pictures_kept;

static bool duskPicture(int max_w, int max_h, DocImage *out)
{
    static char data[1 << 20];
    FILE *f = fopen("/tmp/kaireader_preview/Backgrounds/dusk.png", "rb");
    size_t n = f ? fread(data, 1, sizeof(data), f) : 0;
    if (f)
        fclose(f);
    return n && docImageFromData(data, n, max_w, max_h, out);
}

bool newsJobPictures(const char (*urls)[FEED_URL_MAX], const char (*files)[NEWSJOB_PICTURE_PATH], int count,
                     bool any_certificate, int max_w)
{
    (void)urls;
    (void)any_certificate;
    g_pictures_kept = 0;
    for (int i = 0; i < count; i++) {
        DocImage image;
        if (duskPicture(max_w, max_w * 3, &image)) {
            g_pictures_kept += docImageSaveJpeg(&image, files[i], 82);
            docImageFree(&image);
        }
    }
    g_pictures_ready = true;
    return true;
}

bool newsJobPicturesBusy(void)
{
    return false;
}

bool newsJobTakePictures(int *kept)
{
    if (!g_pictures_ready)
        return false;
    g_pictures_ready = false;
    *kept = g_pictures_kept;
    return true;
}

// A news article's cover: the dusk picture for some (the ones whose hash ends in 0-7), none for the rest.
bool previewNewsCover(const char *path, int w, int h, DocImage *out)
{
    char last = path[strlen(path) - 1];
    return last >= '0' && last <= '7' && duskPicture(w, h, out);
}
