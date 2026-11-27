// News: the RSS and Atom feeds you follow, their articles read as books,
// after Free books when X goes round (only once a feed is followed). The
// feeds are in feeds.txt; each one's articles are kept on the card
// (news/<feed hash>.xml, the last FEED_KEEP), so they're read offline too,
// and checked again once a session when News is first shown, or with Y.
// The library is newest first, all the feeds or one (L and R); an article
// not opened yet wears a dot. Opened, the article is made a page
// (news/articles/<hash>.xhtml, article.c); "Read the full article" in the
// reader's + menu brings its whole page from the site. Also Settings > News:
// add a feed (or a page that links to one), import an OPML file, and each
// feed's name, certificate check and removal.
#include "app.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "approot.h"
#include "gfx.h"
#include "article.h"
#include "feedlist.h"
#include "feeds.h"
#include "feedsuggest.h"
#include "languages.h"
#include "keyboard_system.h"
#include "lang.h"
#include "newsjob.h"
#include "opdsnet.h"

#define NEWS_FOLDER "news"
#define ARTICLES_FOLDER "news/articles"
#define NEWS_SHELF_FILE "shelf-news.txt"
#define OPML_FILE "feeds.opml"
#define OPML_MAX (2u << 20)
#define KEPT_MAX (12u << 20)
#define PATH_MAX_LEN (APP_FOLDER_MAX + 128)

static int g_feed_count;  // For appSourceSet, which has no app.

static void pathIn(const App *app, const char *name, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", app->folder, name);
}

// The news' shelf: the one shown, or kept aside while another library is.
static Shelf *newsShelf(App *app)
{
    return app->lib_tab == LIB_NEWS ? &app->shelf : &app->stash[LIB_NEWS];
}

static void saveNewsShelf(App *app)
{
    char file[PATH_MAX_LEN];
    pathIn(app, NEWS_SHELF_FILE, file, sizeof(file));
    shelfSave(newsShelf(app), file);
}

// A feed's name as shown: its own, else its address's host.
static void feedName(const FeedEntry *e, char *out, size_t size)
{
    if (e->name[0]) {
        snprintf(out, size, "%s", e->name);
        return;
    }
    const char *host = strstr(e->url, "://");
    host = host ? host + 3 : e->url;
    snprintf(out, size, "%.*s", (int)strcspn(host, "/?#"), host);
}

static char *readFile(const char *path, size_t max, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    char *data = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long n = ftell(f);
        if (n >= 0 && (size_t)n <= max && fseek(f, 0, SEEK_SET) == 0) {
            data = malloc((size_t)n + 1);
            if (data && fread(data, 1, (size_t)n, f) == (size_t)n) {
                data[n] = '\0';
                *size = (size_t)n;
            } else {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(f);
    return data;
}

static bool writeFile(const char *path, const char *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

// The suggested feeds of the language chosen, as their page shows them.
static void pushSuggestions(const App *app)
{
    const FeedSuggestion *list;
    int count = feedSuggestions(app->news_suggest_lang, &list);
    const char *names[FEED_SUGGEST_MAX], *urls[FEED_SUGGEST_MAX];
    bool followed[FEED_SUGGEST_MAX];
    for (int i = 0; i < count; i++) {
        char hash[FEED_HASH_LEN + 1];
        feedsHash(list[i].url, hash);
        names[i] = list[i].name;
        urls[i] = list[i].url;
        followed[i] = feedListFind(&app->feeds, hash) >= 0;
    }
    const char *id = feedSuggestLanguage(app->news_suggest_lang);
    settingsSetNewsSuggestions(languageName(languageIndex(id)), count, names, urls, followed);
}

void appNewsFeedSettings(const App *app)
{
    pushSuggestions(app);
    int at = app->news_settings_feed;
    if (at < 0 || at >= app->feeds.count) {
        settingsSetNews(app->feeds.count, "", true);
        return;
    }
    char name[FEED_TEXT_MAX];
    feedName(&app->feeds.items[at], name, sizeof(name));
    settingsSetNews(app->feeds.count, name, !app->feeds.items[at].any_certificate);
}

static void saveFeeds(App *app, uint64_t now_ns)
{
    char file[PATH_MAX_LEN];
    pathIn(app, FEEDLIST_FILE, file, sizeof(file));
    if (!feedListSave(&app->feeds, file))
        appNotify(app, NOTICE_ERROR, T("It couldn't be written to the card."), now_ns);
    g_feed_count = app->feeds.count;
    if (app->news_settings_feed >= app->feeds.count)
        app->news_settings_feed = app->feeds.count - 1;
    if (app->news_settings_feed < 0)
        app->news_settings_feed = 0;
    if (app->prefs.news_feed >= app->feeds.count)
        app->prefs.news_feed = -1;
    appNewsFeedSettings(app);
}

// Articles' pages, whole pages and pictures not made for NEWS_MARK_DAYS go (made again if opened).
static void forgetOldPages(const char *folder)
{
    DIR *dir = opendir(folder);
    if (!dir)
        return;
    time_t limit = time(NULL) - (time_t)NEWS_MARK_DAYS * 86400;
    struct dirent *e;
    while ((e = readdir(dir))) {
        size_t n = strlen(e->d_name);
        bool page = n > 6 && !strcmp(e->d_name + n - 6, ".xhtml"), html = n > 5 && !strcmp(e->d_name + n - 5, ".html");
        bool picture = (n > 4 && !strcmp(e->d_name + n - 4, ".jpg")) || (n > 5 && !strcmp(e->d_name + n - 5, ".none")) ||
                       (n > 5 && !strcmp(e->d_name + n - 5, ".part"));
        if (!page && !html && !picture)  // Its page, its whole page, its pictures.
            continue;
        char path[PATH_MAX_LEN + 300];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", folder, e->d_name);
        if (stat(path, &st) == 0 && st.st_mtime < limit)
            remove(path);
    }
    closedir(dir);
}

void appNewsStart(App *app)
{
    char path[PATH_MAX_LEN];
    pathIn(app, NEWS_FOLDER, path, sizeof(path));
    appRootMakeFolder(path);
    pathIn(app, ARTICLES_FOLDER, path, sizeof(path));
    appRootMakeFolder(path);
    forgetOldPages(path);
    pathIn(app, FEEDLIST_FILE, path, sizeof(path));
    feedListLoad(&app->feeds, path);
    g_feed_count = app->feeds.count;
    pathIn(app, NEWS_SHELF_FILE, path, sizeof(path));
    Shelf *s = &app->stash[LIB_NEWS];
    shelfLoad(s, path);
    for (int i = 0; i < s->count; i++)  // Shown once their feeds, kept on the card, are read.
        s->items[i].present = false;
    if (app->prefs.news_feed >= app->feeds.count)
        app->prefs.news_feed = -1;
    app->news_settings_feed = 0;
    appNewsFeedSettings(app);
}

bool appNewsSet(void)
{
    return g_feed_count > 0;
}

bool appNewsShown(const App *app, const ShelfBook *b)
{
    if (!b->present)
        return false;
    int at = app->prefs.news_feed;
    if (at < 0 || at >= app->feeds.count)
        return true;
    char feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1];
    return newsPathParts(b->path, feed, item) && !strcmp(feed, app->feeds.items[at].hash);
}

// --- Checking the feeds ---

static void startCheck(App *app, uint64_t now_ns, bool say)
{
    if (app->feeds.count == 0)
        return;
    NewsJobFeed *feeds = calloc((size_t)app->feeds.count, sizeof(*feeds));
    if (!feeds)
        return;
    for (int i = 0; i < app->feeds.count; i++) {
        snprintf(feeds[i].url, sizeof(feeds[i].url), "%s", app->feeds.items[i].url);
        snprintf(feeds[i].hash, sizeof(feeds[i].hash), "%s", app->feeds.items[i].hash);
        feeds[i].any_certificate = app->feeds.items[i].any_certificate;
    }
    appNetworkWantNews();
    char folder[PATH_MAX_LEN];
    pathIn(app, NEWS_FOLDER, folder, sizeof(folder));
    bool started = newsJobCheck(feeds, app->feeds.count, folder);
    free(feeds);
    if (!started) {
        if (say)
            appNotify(app, NOTICE_INFO, T("The feeds are being checked already."), now_ns);
        return;
    }
    app->news_checked = true;
    app->news_problem[0] = '\0';
}

void appNewsEntered(App *app, uint64_t now_ns)
{
    if (!app->news_checked)
        startCheck(app, now_ns, false);
}

void appNewsRefresh(App *app, uint64_t now_ns)
{
    startCheck(app, now_ns, true);
}

bool appNewsChecking(int *done, int *count)
{
    return newsJobChecking(done, count);
}

// The selected article's path, to find it again after the shelf is made anew.
static void selectedPath(const App *app, char *out, size_t size)
{
    out[0] = '\0';
    if (app->lib_tab == LIB_NEWS && app->cursor >= 0 && app->cursor < app->order_count)
        snprintf(out, size, "%s", app->shelf.items[app->order[app->cursor]].path);
}

static void selectPath(App *app, const char *path)
{
    for (int i = 0; path[0] && i < app->order_count; i++)
        if (!strcmp(app->shelf.items[app->order[i]].path, path)) {
            app->cursor = i;
            break;
        }
    appResort(app);
}

static void copyMarks(ShelfBook *to, const ShelfBook *from)
{
    to->place = from->place;
    to->percent = from->percent;
    to->last_read = from->last_read;
    to->favorite = from->favorite;
}

// The articles come: the shelf made anew in their order, the read marks kept.
static void takeCheck(App *app, uint64_t now_ns)
{
    NewsArticle *list;
    NewsFeedState *states;
    int count, state_count;
    bool asked;
    if (!newsJobTakeCheck(&list, &count, &states, &state_count, &asked))
        return;
    char selected[SHELF_PATH_MAX], open[SHELF_PATH_MAX] = "";
    selectedPath(app, selected, sizeof(selected));
    Shelf *old = newsShelf(app);
    if (app->book >= 0 && app->lib_tab == LIB_NEWS)
        snprintf(open, sizeof(open), "%s", app->shelf.items[app->book].path);
    Shelf fresh = {0};
    for (int i = 0; i < count; i++) {
        const NewsArticle *a = &list[i];
        char feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1];
        int at = newsPathParts(a->path, feed, item) ? feedListFind(&app->feeds, feed) : -1;
        if (at < 0 || shelfFind(&fresh, a->path))
            continue;  // A feed removed meanwhile, or an article twice.
        ShelfBook *b = shelfAdd(&fresh, a->path);
        if (!b)
            break;
        const ShelfBook *was = shelfFind(old, a->path);
        if (was)
            copyMarks(b, was);
        b->size = NEWS_COVER_CARD;  // Its card's picture (kept at the card's size).
        char name[FEED_TEXT_MAX], date[24];
        feedName(&app->feeds.items[at], name, sizeof(name));
        if (!app->feeds.items[at].name[0] && states && a->feed >= 0 && a->feed < state_count &&
            states[a->feed].title[0])
            snprintf(name, sizeof(name), "%s", states[a->feed].title);
        feedsDateTimeText(a->date, date, sizeof(date));
        shelfSetInfo(b, a->title[0] ? a->title : T("Untitled article"), name);
        // An article's genre is its date and time (UTC); its author_sort, its first words (the showcase's).
        snprintf(b->genre, sizeof(b->genre), "%s", date);
        snprintf(b->author_sort, sizeof(b->author_sort), "%s", a->summary);
        opdsRememberCover(a->path, a->image);
        char lead[SHELF_PATH_MAX + 8];  // Its picture at the lead story's size, kept apart.
        snprintf(lead, sizeof(lead), "%s%s", a->path, NEWS_LEAD_COVER);
        opdsRememberCover(lead, a->image);
    }
    for (int i = 0; i < old->count; i++) {  // The marks of articles not kept any more, for a while.
        const ShelfBook *was = &old->items[i];
        bool keep = was->percent >= 0 || was->favorite || !strcmp(was->path, open);
        if (!keep || shelfFind(&fresh, was->path))
            continue;
        ShelfBook *b = shelfAdd(&fresh, was->path);
        if (!b)
            break;
        *b = *was;
        b->present = false;
    }
    newsForgetMarks(&fresh, (int64_t)time(NULL));
    shelfFree(old);
    *old = fresh;
    if (open[0]) {  // The article being read: its place in the new shelf.
        for (int i = 0; i < old->count; i++)
            if (!strcmp(old->items[i].path, open))
                app->book = i;
    }
    if (asked && states) {  // Why a feed couldn't be read; all of them: said where the articles would be.
        int failed = 0, followed = 0, first = -1, first_feed = -1;  // Of the feeds still followed.
        for (int i = 0; i < state_count; i++) {
            int at = feedListFind(&app->feeds, states[i].hash);
            if (at < 0)
                continue;
            followed++;
            if (!states[i].ok && first < 0) {
                first = i;
                first_feed = at;
            }
            failed += !states[i].ok;
        }
        app->news_problem[0] = '\0';
        if (failed && failed == followed)
            snprintf(app->news_problem, sizeof(app->news_problem), "%s", states[first].why);
        if (failed && app->lib_tab == LIB_NEWS) {
            char name[FEED_TEXT_MAX], text[FEED_TEXT_MAX + NETJOB_WHY_MAX + 8];
            feedName(&app->feeds.items[first_feed], name, sizeof(name));
            snprintf(text, sizeof(text), "%s: %s", name, states[first].why);
            appNotify(app, NOTICE_ERROR, text, now_ns);
        }
    }
    free(list);
    free(states);
    if (app->lib_tab == LIB_NEWS && app->view == VIEW_LIBRARY) {
        app->order_count = 0;  // The order was the old shelf's.
        appResort(app);
        selectPath(app, selected);
    } else if (app->lib_tab == LIB_NEWS) {
        app->order_count = 0;
        appResort(app);
    }
    saveNewsShelf(app);
}

// --- The library ---

#define NEWS_SIDE 32  // The showcase's margins (ui_common.h's MARGIN).

bool appNewsShowcase(const App *app)
{
    return app->lib_tab == LIB_NEWS && !app->prefs.list_view;
}

int appNewsColumns(void)
{
    return gfxLayoutWidth() >= 1000 ? 4 : 2;
}

void appNewsCardSize(int *w, int *picture_h)
{
    int cols = appNewsColumns();
    *w = (gfxLayoutWidth() - 2 * NEWS_SIDE - (cols - 1) * NEWS_GAP) / cols;
    *picture_h = *w * 9 / 16;
}

int appNewsRowH(void)
{
    int w, ph;
    appNewsCardSize(&w, &ph);
    return ph + NEWS_CARD_TEXT + NEWS_GAP;
}

int appNewsLeadH(void)
{
    if (gfxLayoutWidth() >= 1000)  // Its picture at the right, its words beside it.
        return (gfxLayoutWidth() - 2 * NEWS_SIDE) * NEWS_LEAD_SHARE / 100 * 9 / 16 + 20 + NEWS_GAP;
    int w = gfxLayoutWidth() - 2 * NEWS_SIDE;  // Upright: its picture across, its words under it.
    return w * 9 / 16 + 230 + NEWS_GAP;
}

void appNewsStepFeed(App *app, int dir)
{
    int n = app->feeds.count + 1;  // All the feeds (-1), then each.
    app->prefs.news_feed = ((app->prefs.news_feed + 1 + dir) % n + n) % n - 1;
    appSavePrefs(app);
    app->cursor = 0;
    app->scroll = 0;
    appResort(app);
}

const char *appNewsFeedName(const App *app)
{
    static char name[FEED_TEXT_MAX];
    int at = app->prefs.news_feed;
    if (at < 0 || at >= app->feeds.count)
        return T("All the feeds");
    feedName(&app->feeds.items[at], name, sizeof(name));
    return name;
}

const char *appNewsNextFeedLine(const App *app)
{
    static char line[FEED_TEXT_MAX + 64];
    int n = app->feeds.count + 1;
    int next = ((app->prefs.news_feed + 2) % n + n) % n - 1;
    if (next < 0)
        return T("Show all the feeds");
    char name[FEED_TEXT_MAX];
    feedName(&app->feeds.items[next], name, sizeof(name));
    snprintf(line, sizeof(line), T("Show %s"), name);
    return line;
}

void appNewsMarkAllRead(App *app, uint64_t now_ns)
{
    if (app->lib_tab != LIB_NEWS)
        return;
    int marked = 0;
    for (int i = 0; i < app->order_count; i++) {
        ShelfBook *b = &app->shelf.items[app->order[i]];
        if (b->percent < 0) {
            shelfMarkRead(b, (DocPlace){0, 0}, 100, (int64_t)time(NULL));
            marked++;
        }
    }
    appNotify(app, NOTICE_INFO, marked ? T("Every article shown is marked as read.") : T("There's nothing new to mark."),
              now_ns);
    if (marked)
        saveNewsShelf(app);
}

// --- Reading ---

// An article of the library: its feed, and the article itself from what's kept on the card.
static bool findArticle(App *app, const ShelfBook *b, int *feed, FeedItem *out, char **html, uint64_t now_ns)
{
    char feed_hash[FEED_HASH_LEN + 1], item_hash[FEED_HASH_LEN + 1];
    *html = NULL;
    *feed = newsPathParts(b->path, feed_hash, item_hash) ? feedListFind(&app->feeds, feed_hash) : -1;
    if (*feed < 0) {
        appNotify(app, NOTICE_ERROR, T("That article's feed isn't followed any more."), now_ns);
        return false;
    }
    char path[PATH_MAX_LEN], name[64];
    snprintf(name, sizeof(name), "%s/%s.xml", NEWS_FOLDER, feed_hash);
    pathIn(app, name, path, sizeof(path));
    size_t size;
    char *data = readFile(path, KEPT_MAX, &size);
    Feed f;
    bool found = false;
    if (data && feedsParse(data, size, app->feeds.items[*feed].url, &f)) {
        for (int i = 0; i < f.count && !found; i++) {
            char hash[FEED_HASH_LEN + 1];
            feedsHash(f.items[i].id, hash);
            if (strcmp(hash, item_hash))
                continue;
            *out = f.items[i];
            *html = f.items[i].html;
            f.items[i].html = NULL;
            out->html = NULL;
            found = true;
        }
        feedsFree(&f);
    }
    free(data);
    if (!found)
        appNotify(app, NOTICE_ERROR, T("That article isn't kept on the card any more."), now_ns);
    return found;
}

// Where an article's page goes: news/articles/<hash>.xhtml (its whole page: <hash>-full.xhtml).
static bool pagePath(const App *app, const char *path, bool full, char *out, size_t size)
{
    char feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1], name[96];
    if (!newsPathParts(path, feed, item))
        return false;
    snprintf(name, sizeof(name), "%s/%s%s.xhtml", ARTICLES_FOLDER, item, full ? "-full" : "");
    pathIn(app, name, out, size);
    return true;
}

// An article's whole page, as brought from its site (its address on the first line, then the
// part that reads as the article): news/articles/<hash>-full.html, kept to make its page again.
static bool sidecarPath(const App *app, const char *path, char *out, size_t size)
{
    char feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1], name[96];
    if (!newsPathParts(path, feed, item))
        return false;
    snprintf(name, sizeof(name), "%s/%s-full.html", ARTICLES_FOLDER, item);
    pathIn(app, name, out, size);
    return true;
}

// The pictures of a page being made: where they go, and the ones not on the card yet.
typedef struct {
    const App *app;
    const char *base;   // The article's address (its pictures' src are relative to it).
    char item[FEED_HASH_LEN + 1];
    char urls[NEWSJOB_PICTURES][FEED_URL_MAX];
    char files[NEWSJOB_PICTURES][NEWSJOB_PICTURE_PATH];
    int missing;
} Pictures;

static ArticlePicture pictureFile(void *user, const char *src, char *file, size_t size)
{
    Pictures *p = user;
    char url[FEED_URL_MAX], hash[FEED_HASH_LEN + 1], name[96], path[NEWSJOB_PICTURE_PATH], none[NEWSJOB_PICTURE_PATH + 8];
    if (!opdsResolve(p->base, src, url, sizeof(url)) || (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)))
        return ARTICLE_PICTURE_NONE;
    feedsHash(url, hash);
    snprintf(name, sizeof(name), "%s-%s.jpg", p->item, hash);
    char rel[128];
    snprintf(rel, sizeof(rel), "%s/%s", ARTICLES_FOLDER, name);
    pathIn(p->app, rel, path, sizeof(path));
    struct stat st;
    if (stat(path, &st) == 0) {
        if (st.st_size == 0)
            return ARTICLE_PICTURE_OMIT;  // Too small to be a picture (a tracking pixel).
        snprintf(file, size, "%s", name);  // Next to the page.
        return ARTICLE_PICTURE_FILE;
    }
    snprintf(none, sizeof(none), "%s.none", path);
    if (stat(none, &st) != 0 && p->missing < NEWSJOB_PICTURES) {  // To fetch (once is enough to know).
        bool listed = false;
        for (int i = 0; i < p->missing && !listed; i++)
            listed = !strcmp(p->files[i], path);
        if (!listed) {
            snprintf(p->urls[p->missing], FEED_URL_MAX, "%s", url);
            snprintf(p->files[p->missing], NEWSJOB_PICTURE_PATH, "%s", path);
            p->missing++;
        }
    }
    return ARTICLE_PICTURE_NONE;
}

// The column's width in pixels: the pictures are kept no wider.
static int pictureWidth(const App *app)
{
    int x, y, w, h;
    appColumn(app, 0, &x, &y, &w, &h);
    int px = (int)(w * GFX_SCALE * gfxDetail());
    return px > 1600 ? 1600 : px < 200 ? 200 : px;
}

// An article's page written to local: its HTML (the feed's, or its whole page's), with the pictures
// on the card shown; the ones not yet are asked for (and the page is made again when they come).
static bool writePage(App *app, const ShelfBook *b, const char *html, const char *base, bool full, bool has_link,
                      bool any_certificate, bool fetch, const char *local, uint64_t now_ns)
{
    char byline[SHELF_TEXT_MAX * 2 + 8];
    snprintf(byline, sizeof(byline), "%s%s%.10s", b->author, b->genre[0] ? " \xC2\xB7 " : "", b->genre);
    const char *note = full ? T("From the article's page on its site: the part of it that reads as the article.")
                       : has_link ? T("This is what the feed gives: \"Read the full article\" in the + menu brings the whole of it from its site.")
                                  : NULL;
    Pictures *pics = calloc(1, sizeof(*pics));  // Big: not on the stack.
    char feed_hash[FEED_HASH_LEN + 1];
    if (pics) {
        pics->app = app;
        pics->base = base ? base : "";
        if (!newsPathParts(b->path, feed_hash, pics->item))
            pics->item[0] = '\0';
    }
    ArticleParts parts = {b->title, byline, html, T("[picture]"), note, pics && pics->item[0] ? pictureFile : NULL, pics};
    size_t size;
    char *page = articlePage(&parts, &size);
    bool ok = page && writeFile(local, page, size);
    free(page);
    if (!ok)
        appNotify(app, NOTICE_ERROR, T("It couldn't be written to the card."), now_ns);
    if (fetch && ok && pics && pics->missing > 0 && !newsJobPicturesBusy()) {
        appNetworkWantNews();
        if (newsJobPictures((const char(*)[FEED_URL_MAX])pics->urls, (const char(*)[NEWSJOB_PICTURE_PATH])pics->files,
                            pics->missing, any_certificate, pictureWidth(app))) {
            snprintf(app->news_pictures, sizeof(app->news_pictures), "%s", b->path);
            appNotify(app, NOTICE_INFO, T("Loading the pictures..."), now_ns);
        }
    }
    free(pics);
    return ok;
}

// An article's page made (again) on the card: from its whole page if it was brought, else the feed's.
static bool makePage(App *app, const ShelfBook *b, bool fetch, char *local, size_t size, uint64_t now_ns)
{
    char sidecar[PATH_MAX_LEN];
    size_t length;
    char *kept = sidecarPath(app, b->path, sidecar, sizeof(sidecar)) ? readFile(sidecar, ARTICLE_MAX, &length) : NULL;
    char feed_hash[FEED_HASH_LEN + 1], item_hash[FEED_HASH_LEN + 1];
    int feed = newsPathParts(b->path, feed_hash, item_hash) ? feedListFind(&app->feeds, feed_hash) : -1;
    bool any = feed >= 0 && app->feeds.items[feed].any_certificate;
    if (kept) {  // "address\nhtml".
        char *html = strchr(kept, '\n');
        bool ok = html && pagePath(app, b->path, true, local, size);
        if (ok) {
            *html++ = '\0';
            ok = writePage(app, b, html, kept, true, true, any, fetch, local, now_ns);
        }
        free(kept);
        if (ok) {
            app->news_open_full = true;
            app->news_open_link = true;
            return true;
        }
    }
    if (!pagePath(app, b->path, false, local, size))
        return false;
    FeedItem item;
    char *html;
    if (!findArticle(app, b, &feed, &item, &html, now_ns))
        return false;
    bool ok = writePage(app, b, html, item.link[0] ? item.link : app->feeds.items[feed].url, false, item.link[0] != '\0',
                        app->feeds.items[feed].any_certificate, fetch, local, now_ns);
    free(html);
    app->news_open_full = false;
    app->news_open_link = item.link[0] != '\0';
    return ok;
}

bool appNewsCanReadFull(const App *app)
{
    return appNewsReading(app) && !app->news_open_full && app->news_open_link && !app->news_full[0];
}

bool appNewsReady(App *app, int book, char *local, size_t size, uint64_t now_ns)
{
    return makePage(app, &app->shelf.items[book], true, local, size, now_ns);
}

bool appNewsReading(const App *app)
{
    return app->view == VIEW_READER && app->lib_tab == LIB_NEWS && app->book >= 0 &&
           app->book < app->shelf.count && !strncmp(app->shelf.items[app->book].path, NEWS_PREFIX, strlen(NEWS_PREFIX));
}

void appNewsFull(App *app, uint64_t now_ns)
{
    if (!appNewsReading(app))
        return;
    const ShelfBook *b = &app->shelf.items[app->book];
    FeedItem item;
    char *html;
    int feed;
    if (!findArticle(app, b, &feed, &item, &html, now_ns))
        return;
    free(html);
    if (!item.link[0]) {
        appNotify(app, NOTICE_INFO, T("This article has no page on its site."), now_ns);
        return;
    }
    appNetworkWantNews();
    if (!newsJobFull(item.link, app->feeds.items[feed].any_certificate)) {
        appNotify(app, NOTICE_INFO, T("An article is being brought already."), now_ns);
        return;
    }
    snprintf(app->news_full, sizeof(app->news_full), "%s", b->path);
    snprintf(app->news_full_link, sizeof(app->news_full_link), "%s", item.link);
    appNotify(app, NOTICE_INFO, T("Bringing the whole article from its site..."), now_ns);
}

// The article being read made again (its pictures came, or its whole page), where it's read.
static void reopenArticle(App *app, const char *path, bool at_start, bool fetch, uint64_t now_ns)
{
    ShelfBook *b = shelfFind(&app->shelf, path);
    char local[PATH_MAX_LEN];
    if (!b || !makePage(app, b, fetch, local, sizeof(local), now_ns))
        return;
    appReopenBook(app, local, at_start);
}

// The whole article come: kept, its page written, and opened again at its start if it's being read.
static void takeFull(App *app, uint64_t now_ns)
{
    char *html, why[NETJOB_WHY_MAX];
    bool ok;
    if (!newsJobTakeFull(&html, &ok, why, sizeof(why)))
        return;
    char path[SHELF_PATH_MAX];
    snprintf(path, sizeof(path), "%s", app->news_full);
    app->news_full[0] = '\0';
    if (!ok) {
        appNotify(app, NOTICE_ERROR, why, now_ns);
        free(html);
        return;
    }
    char sidecar[PATH_MAX_LEN];
    FILE *f = sidecarPath(app, path, sidecar, sizeof(sidecar)) ? fopen(sidecar, "wb") : NULL;
    bool written = f && fprintf(f, "%s\n%s", app->news_full_link, html) > 0;
    if (f)
        written = fclose(f) == 0 && written;
    free(html);
    if (!written) {
        remove(sidecar);
        appNotify(app, NOTICE_ERROR, T("It couldn't be written to the card."), now_ns);
        return;
    }
    if (!(appNewsReading(app) && !strcmp(app->shelf.items[app->book].path, path))) {
        appNotify(app, NOTICE_DONE, T("The whole article is here: it opens with it from now on."), now_ns);
        return;
    }
    reopenArticle(app, path, true, true, now_ns);
    appNotify(app, NOTICE_DONE, T("The whole article, from its site."), now_ns);
}

// An article's pictures come: shown at once if it's being read (where it's read).
static void takePictures(App *app, uint64_t now_ns)
{
    int kept;
    if ((appNewsReading(app) && !app->ready) || !newsJobTakePictures(&kept))
        return;  // The article still opening: they wait for it.
    char path[SHELF_PATH_MAX];
    snprintf(path, sizeof(path), "%s", app->news_pictures);
    app->news_pictures[0] = '\0';
    if (kept > 0 && appNewsReading(app) && !strcmp(app->shelf.items[app->book].path, path) && app->ready)
        reopenArticle(app, path, false, false, now_ns);
}

// --- Settings > News ---

void appNewsOpenSettings(App *app)
{
    appOpenSettingsPage(app, SETTINGS_PAGE_NEWS);
    app->settings_cursor = 0;  // Add a feed.
}

// A suggestion being looked for: followed by its own address and name (not the feed's title).
static char g_pending_url[FEED_URL_MAX], g_pending_name[FEED_TEXT_MAX];

// url looked for (a feed, or a page that links to one), then followed (takeFind).
static void findFeed(App *app, const char *url, uint64_t now_ns)
{
    appNetworkWantNews();
    if (!newsJobFind(url)) {
        appNotify(app, NOTICE_INFO, T("A feed is being looked for already."), now_ns);
        return;
    }
    appNotify(app, NOTICE_INFO, T("Looking for the feed..."), now_ns);
}

// A suggested feed (its row on the page): followed as a typed one is, under its name.
static void followSuggestion(App *app, int index, uint64_t now_ns)
{
    const FeedSuggestion *list;
    int count = feedSuggestions(app->news_suggest_lang, &list);
    if (index < 0 || index >= count)
        return;
    char hash[FEED_HASH_LEN + 1];
    feedsHash(list[index].url, hash);
    if (feedListFind(&app->feeds, hash) >= 0) {
        appNotify(app, NOTICE_INFO, T("That feed is followed already."), now_ns);
        return;
    }
    if (newsJobFinding()) {
        appNotify(app, NOTICE_INFO, T("A feed is being looked for already."), now_ns);
        return;
    }
    snprintf(g_pending_url, sizeof(g_pending_url), "%s", list[index].url);
    snprintf(g_pending_name, sizeof(g_pending_name), "%s", list[index].name);
    findFeed(app, list[index].url, now_ns);
}

static void addFeed(App *app, uint64_t now_ns)
{
    char text[FEED_URL_MAX], url[FEED_URL_MAX];
    SystemKeyboardResult result =
        systemKeyboardInput(T("The feed's address, or the address of a site's page that links to it"), "", text,
                            sizeof(text), sizeof(text) - 16);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now_ns);
        return;
    }
    if (result != SYSTEM_KEYBOARD_OK)
        return;
    const char *t = text + strspn(text, " \t");
    size_t n = strlen(t);
    while (n > 0 && (t[n - 1] == ' ' || t[n - 1] == '\t'))
        n--;
    if (!n)
        return;
    bool scheme = strstr(t, "://") != NULL;
    snprintf(url, sizeof(url), "%s%.*s", scheme ? "" : "http://", (int)n, t);  // http:// goes on to https://.
    if ((strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) || strpbrk(url, " \t") ||
        !url[strcspn(url, ":") + 3]) {
        appNotify(app, NOTICE_ERROR, T("That isn't a web address: harbor.example/rss, http:// or https://."), now_ns);
        return;
    }
    if (newsJobFinding()) {
        appNotify(app, NOTICE_INFO, T("A feed is being looked for already."), now_ns);
        return;
    }
    g_pending_url[0] = g_pending_name[0] = '\0';
    findFeed(app, url, now_ns);
}

static void takeFind(App *app, uint64_t now_ns)
{
    char url[FEED_URL_MAX], title[FEED_TEXT_MAX], why[NETJOB_WHY_MAX];
    bool ok;
    if (!newsJobTakeFind(url, sizeof(url), title, sizeof(title), &ok, why, sizeof(why)))
        return;
    if (ok && g_pending_url[0]) {  // A suggestion: kept by the address it's suggested at, with its name.
        snprintf(url, sizeof(url), "%s", g_pending_url);
        snprintf(title, sizeof(title), "%s", g_pending_name);
    }
    g_pending_url[0] = g_pending_name[0] = '\0';
    if (!ok) {
        appNotify(app, NOTICE_ERROR, why, now_ns);
        return;
    }
    bool added;
    int at = feedListAdd(&app->feeds, url, title, &added);
    if (at < 0) {
        appNotify(app, NOTICE_ERROR, T("No more feeds can be followed."), now_ns);
        return;
    }
    app->news_settings_feed = at;
    if (!added) {
        appNewsFeedSettings(app);
        appNotify(app, NOTICE_INFO, T("That feed is followed already."), now_ns);
        return;
    }
    saveFeeds(app, now_ns);
    char name[FEED_TEXT_MAX], text[FEED_TEXT_MAX + 96];
    feedName(&app->feeds.items[at], name, sizeof(name));
    snprintf(text, sizeof(text), T("%s: added. X in the library goes to the news."), name);
    appNotify(app, NOTICE_DONE, text, now_ns);
    app->news_checked = false;  // Its articles: asked for now.
    startCheck(app, now_ns, false);
}

static void importOpml(App *app, uint64_t now_ns)
{
    char path[PATH_MAX_LEN];
    pathIn(app, OPML_FILE, path, sizeof(path));
    size_t size;
    char *data = readFile(path, OPML_MAX, &size);
    if (!data) {
        appNotify(app, NOTICE_INFO, T("Put feeds.opml in KaiReader's folder (sdmc:/switch/kaireader/config) first."),
                  now_ns);
        return;
    }
    FeedOutline *outlines;
    int count = feedsParseOpml(data, size, &outlines);
    free(data);
    if (count < 0) {
        appNotify(app, NOTICE_ERROR, T("That file isn't an OPML list of feeds."), now_ns);
        return;
    }
    int added = 0;
    for (int i = 0; i < count; i++) {
        bool fresh;
        if (feedListAdd(&app->feeds, outlines[i].url, outlines[i].title, &fresh) >= 0 && fresh)
            added++;
    }
    free(outlines);
    char text[96];
    snprintf(text, sizeof(text), T("Feeds added: %d."), added);
    appNotify(app, added ? NOTICE_DONE : NOTICE_INFO, text, now_ns);
    if (!added)
        return;
    saveFeeds(app, now_ns);
    app->news_checked = false;
    startCheck(app, now_ns, false);
}

static void renameFeed(App *app, int at, uint64_t now_ns)
{
    char name[FEED_TEXT_MAX], typed[FEED_TEXT_MAX];
    feedName(&app->feeds.items[at], name, sizeof(name));
    SystemKeyboardResult result = systemKeyboardInput(T("The feed's name"), name, typed, sizeof(typed), 64);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now_ns);
        return;
    }
    if (result != SYSTEM_KEYBOARD_OK || !typed[strspn(typed, " ")])
        return;
    feedListRename(&app->feeds, at, typed);
    saveFeeds(app, now_ns);
    Shelf *s = newsShelf(app);  // Its articles say it at once.
    for (int i = 0; i < s->count; i++) {
        char feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1];
        if (newsPathParts(s->items[i].path, feed, item) && !strcmp(feed, app->feeds.items[at].hash))
            snprintf(s->items[i].author, sizeof(s->items[i].author), "%s", app->feeds.items[at].name);
    }
}

static void removeFeed(App *app, int at, uint64_t now_ns)
{
    FeedEntry gone = app->feeds.items[at];
    char name[FEED_TEXT_MAX], file[64], path[PATH_MAX_LEN];
    feedName(&gone, name, sizeof(name));
    snprintf(file, sizeof(file), "%s/%s.xml", NEWS_FOLDER, gone.hash);
    pathIn(app, file, path, sizeof(path));
    remove(path);
    feedListRemove(&app->feeds, at);
    if (app->prefs.news_feed == at)
        app->prefs.news_feed = -1;
    else if (app->prefs.news_feed > at)
        app->prefs.news_feed--;
    saveFeeds(app, now_ns);
    appSavePrefs(app);
    Shelf *s = newsShelf(app);  // Its articles leave the library.
    for (int i = 0; i < s->count; i++) {
        char feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1];
        if (newsPathParts(s->items[i].path, feed, item) && !strcmp(feed, gone.hash)) {
            s->items[i].present = false;
            s->items[i].percent = -1;
            s->items[i].favorite = false;
        }
    }
    saveNewsShelf(app);
    if (app->lib_tab == LIB_NEWS && app->feeds.count == 0 && app->view == VIEW_LIBRARY)
        appLibTab(app, LIB_CARD, now_ns);
    else if (app->lib_tab == LIB_NEWS && app->view == VIEW_LIBRARY)
        appResort(app);
    char text[FEED_TEXT_MAX + 64];
    snprintf(text, sizeof(text), T("%s: no longer followed."), name);
    appNotify(app, NOTICE_DONE, text, now_ns);
}

bool appNewsActivate(App *app, SettingRow row, int dir, uint64_t now_ns)
{
    int at = app->news_settings_feed;
    bool feed = at >= 0 && at < app->feeds.count;
    bool about_feed = row == SETTING_NEWS_FEED || row == SETTING_NEWS_RENAME || row == SETTING_NEWS_REMOVE ||
                      row == SETTING_NEWS_CERTIFICATE;
    if (about_feed && !feed) {
        if (!dir || row == SETTING_NEWS_FEED || row == SETTING_NEWS_CERTIFICATE)
            appNotify(app, NOTICE_INFO, T("Add a feed first."), now_ns);
        return true;
    }
    switch (row) {
    case SETTING_OPEN_NEWS_SUGGESTED:  // Its page, in the app's language (or English).
        if (dir < 0)
            return true;
        app->news_suggest_lang = feedSuggestLanguageIndex(app->prefs.language);
        if (app->news_suggest_lang < 0)
            app->news_suggest_lang = 0;
        appNewsFeedSettings(app);
        app->settings_page = SETTINGS_PAGE_NEWS_SUGGESTED;
        app->settings_cursor = 1;  // Its first feed.
        return true;
    case SETTING_NEWS_SUGGEST_LANG: {
        int n = feedSuggestLanguages();
        app->news_suggest_lang = ((app->news_suggest_lang + (dir ? dir : 1)) % n + n) % n;
        appNewsFeedSettings(app);
        return true;
    }
    case SETTING_NEWS_FEED: {
        int n = app->feeds.count;
        app->news_settings_feed = ((at + (dir ? dir : 1)) % n + n) % n;
        appNewsFeedSettings(app);
        return true;
    }
    case SETTING_NEWS_CERTIFICATE:
        app->feeds.items[at].any_certificate = !app->feeds.items[at].any_certificate;
        saveFeeds(app, now_ns);
        return true;
    default:
        break;
    }
    if (dir)
        return true;
    switch (row) {
    case SETTING_NEWS_ADD: addFeed(app, now_ns); return true;
    case SETTING_NEWS_IMPORT: importOpml(app, now_ns); return true;
    case SETTING_NEWS_RENAME: renameFeed(app, at, now_ns); return true;
    case SETTING_NEWS_REMOVE: removeFeed(app, at, now_ns); return true;
    default:
        break;
    }
    if (row >= SETTING_NEWS_SUGGEST_0 && row <= SETTING_NEWS_SUGGEST_LAST) {
        followSuggestion(app, row - SETTING_NEWS_SUGGEST_0, now_ns);
        return true;
    }
    switch (row) {
    default: return false;
    }
}

void appNewsUpdate(App *app, uint64_t now_ns)
{
    takeCheck(app, now_ns);
    takeFind(app, now_ns);
    takeFull(app, now_ns);
    takePictures(app, now_ns);
}
