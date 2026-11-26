// feedlist.c and article.c: the feeds followed, read marks, and articles made into pages.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <mupdf/fitz.h>

#include "article.h"
#include "check.h"
#include "feedlist.h"

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

// MuPDF reads the page as XHTML: it must be well formed.
static bool wellFormed(const char *page)
{
    fz_context *ctx = fz_new_context(NULL, NULL, 8u << 20);
    bool ok = false;
    fz_buffer *buf = NULL;
    fz_var(buf);
    fz_var(ok);
    fz_try(ctx)
    {
        buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)page, strlen(page));
        fz_drop_xml(ctx, fz_parse_xml(ctx, buf, 0));
        ok = true;
    }
    fz_always(ctx)
    {
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx)
    {
        ok = false;
    }
    fz_drop_context(ctx);
    return ok;
}

static void testFeedList(void)
{
    mkdir(TMP_ROOT, 0755);
    const char *file = TMP_ROOT "/feeds.txt";
    remove(file);
    FeedList l;
    CHECK(feedListLoad(&l, file) && l.count == 0);  // No file: none.
    bool added;
    CHECK(feedListAdd(&l, "https://harbor.example/rss", "Harbor\tGazette\n", &added) == 0 && added);
    CHECK_STR(l.items[0].name, "Harbor Gazette ");
    CHECK(feedListAdd(&l, "http://192.168.0.168:8099/rss.xml", "", &added) == 1 && added);
    CHECK(feedListAdd(&l, "https://harbor.example/rss", "Again", &added) == 0 && !added);  // Once.
    CHECK(feedListAdd(&l, "", "x", &added) == -1);
    l.items[1].any_certificate = true;
    feedListRename(&l, 1, "Local test");
    CHECK(feedListSave(&l, file));
    FeedList back;
    CHECK(feedListLoad(&back, file) && back.count == 2);
    CHECK(back.count == 2 && !strcmp(back.items[1].name, "Local test") && back.items[1].any_certificate &&
          !back.items[0].any_certificate);
    CHECK(feedListFind(&back, l.items[1].hash) == 1);
    CHECK(feedListFind(&back, "0000000000000000") == -1);
    feedListRemove(&back, 0);
    CHECK(back.count == 1 && !strcmp(back.items[0].url, "http://192.168.0.168:8099/rss.xml"));
    feedListRemove(&back, 5);
    CHECK(back.count == 1);

    // A file written by hand: comments, a line that isn't a feed, no name, no flag.
    FILE *f = fopen(file, "w");
    fputs("# mine\n\nftp://not.a/feed\t1\tNo\nhttps://a.example/feed\nhttps://b.example/rss\t0\n", f);
    fclose(f);
    CHECK(feedListLoad(&back, file) && back.count == 2);
    CHECK(back.count == 2 && !back.items[0].any_certificate && back.items[1].any_certificate &&
          back.items[0].name[0] == '\0');

    FeedList full = {0};
    for (int i = 0; i < FEEDLIST_MAX + 2; i++) {
        char url[64];
        snprintf(url, sizeof(url), "https://f%d.example/rss", i);
        int at = feedListAdd(&full, url, "", &added);
        CHECK(i < FEEDLIST_MAX ? at == i : at == -1);
    }
}

static void testPaths(void)
{
    char path[128], feed[FEED_HASH_LEN + 1], item[FEED_HASH_LEN + 1];
    CHECK(newsPath("0123456789abcdef", "tag:x,1", path, sizeof(path)));
    CHECK(!strncmp(path, "news:0123456789abcdef/", 22) && strlen(path) == 22 + FEED_HASH_LEN);
    CHECK(newsPathParts(path, feed, item));
    CHECK_STR(feed, "0123456789abcdef");
    CHECK(strlen(item) == FEED_HASH_LEN);
    CHECK(!newsPathParts("opds:http://x/1#.epub", feed, item));
    CHECK(!newsPathParts("news:0123/4567", feed, item));
    CHECK(!newsPathParts("news:0123456789abcdef/0123456789abcdeg", feed, item));
    CHECK(!newsPath("0123456789abcdef", "x", path, 10));
}

static void testMarks(void)
{
    Shelf s = {0};
    int64_t now = 2000000000, day = 86400;
    ShelfBook *kept = shelfAdd(&s, "news:a/1");
    ShelfBook *gone_old = shelfAdd(&s, "news:a/2");
    ShelfBook *gone_new = shelfAdd(&s, "news:a/3");
    ShelfBook *unread = shelfAdd(&s, "news:a/4");
    CHECK(kept && gone_old && gone_new && unread);
    if (!kept || !gone_old || !gone_new || !unread)
        return;
    DocPlace start = {0, 0};
    shelfMarkRead(kept, start, 100, now - 90 * day);   // Old, but its article is still kept.
    shelfMarkRead(gone_old, start, 40, now - 31 * day);
    shelfMarkRead(gone_new, start, 100, now - 29 * day);
    gone_old->present = gone_new->present = unread->present = false;
    CHECK(newsForgetMarks(&s, now) == 1);
    CHECK(gone_old->percent == -1 && gone_old->last_read == 0);
    CHECK(kept->percent == 100 && gone_new->percent == 100 && unread->percent == -1);
    shelfFree(&s);
}

static void testArticle(void)
{
    const char *html =
        "<p>First <b>bold</b> &amp; <a href=\"https://x.example/\">a link</a>.</p>"
        "<script>alert(1)</script><style>p{}</style><iframe src=\"x\"></iframe>"
        "<p>A picture: <img src=\"a.jpg\" alt=\"Two gulls\"> and <img src=\"b.jpg\"></p>"
        "<ul><li>one<li>two</ul><form><input name=q></form><font color=red>red</font>"
        "<div onclick=\"x()\" style=\"color:red\">block</div><br><hr>";
    ArticleParts parts = {"Gulls & <Terns>", "Bay Log \xC2\xB7 2025-01-02", html, "[imagen]", "The end."};
    size_t size;
    char *page = articlePage(&parts, &size);
    CHECK(page && size == strlen(page));
    if (page) {
        CHECK(strstr(page, "<h1>Gulls &amp; &lt;Terns&gt;</h1>") != NULL);
        CHECK(strstr(page, "<p class=\"kr-by\">Bay Log \xC2\xB7 2025-01-02</p>") != NULL);
        CHECK(strstr(page, "<p>First <b>bold</b> &amp; a link.</p>") != NULL);
        CHECK(strstr(page, "<i>[Two gulls]</i>") && strstr(page, "<i>[imagen]</i>"));
        CHECK(strstr(page, "<li>one</li><li>two</li>") != NULL);
        CHECK(strstr(page, "red") && strstr(page, "<div>block</div>") && strstr(page, "<br/>") && strstr(page, "<hr/>"));
        CHECK(!strstr(page, "alert") && !strstr(page, "iframe") && !strstr(page, "onclick") && !strstr(page, "<input") &&
              !strstr(page, "a.jpg") && !strstr(page, "href"));
        CHECK(strstr(page, "The end.") != NULL);
        CHECK(wellFormed(page));
    }
    free(page);
    ArticleParts bare = {"Only a title", "", NULL, "[picture]", NULL};
    page = articlePage(&bare, &size);
    CHECK(page && strstr(page, "<h1>Only a title</h1>") && !strstr(page, "class=\"kr-by\""));
    free(page);

    // Deeply nested HTML (a hostile feed): cut to its words past the depth kept, no crash.
    size_t deep_size = 20000 * 5 + 64;
    char *deep = malloc(deep_size);
    if (deep) {
        size_t o = 0;
        for (int i = 0; i < 20000; i++)
            o += (size_t)snprintf(deep + o, deep_size - o, "<div>");
        snprintf(deep + o, deep_size - o, "%s", "deep words");
        ArticleParts d = {"Deep", "", deep, "[picture]", NULL};
        page = articlePage(&d, &size);
        CHECK(page && strstr(page, "deep words") && wellFormed(page));
        free(page);
        free(deep);
    }
}

static void testExtract(void)
{
    const char *page =
        "<html><head><title>x</title><script>var a=1;</script></head><body>"
        "<nav><p>Home | News | Sports</p></nav>"
        "<article><p>Teaser.</p></article>"
        "<div id=\"story\"><h2>The ferry</h2>"
        "<p>The new ferry left the harbor at dawn, carrying forty passengers and a crate of apples.</p>"
        "<p>Its captain said the crossing took less than an hour, even with the wind against them.</p>"
        "<p><img src=\"ferry.jpg\" alt=\"The ferry\"> Next week it sails twice a day.</p></div>"
        "<footer><p>Copyright</p></footer></body></html>";
    char *html = articleExtract(page, strlen(page));
    CHECK(html && strstr(html, "forty passengers") && strstr(html, "twice a day"));
    CHECK(html && !strstr(html, "Home | News") && !strstr(html, "Copyright") && !strstr(html, "var a"));
    CHECK(html && strstr(html, "<img alt=\"The ferry\"/>"));  // Said by articlePage, in the reader's language.
    if (html) {
        ArticleParts parts = {"The ferry", "", html, "[picture]", NULL};
        char *out = articlePage(&parts, NULL);
        CHECK(out && strstr(out, "<i>[The ferry]</i>"));
        free(out);
    }
    free(html);

    const char *with_article =
        "<html><body><main><article><p>"
        "A long article body that is surely the article: it goes on and on about tides, the moon, the harbor "
        "master's logbook and the gulls that follow every boat home in the evening light.</p></article>"
        "</main><aside><p>Related</p></aside></body></html>";
    html = articleExtract(with_article, strlen(with_article));
    CHECK(html && strstr(html, "harbor master") && !strstr(html, "Related"));
    free(html);

    const char *nothing = "<html><body><p>Hi</p></body></html>";
    CHECK(articleExtract(nothing, strlen(nothing)) == NULL);
    CHECK(articleExtract("", 0) == NULL);
}

void testNews(void)
{
    testFeedList();
    testPaths();
    testMarks();
    testArticle();
    testExtract();
}
