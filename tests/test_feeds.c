// feeds.c: RSS, Atom and RDF feeds as sites write them (all made up here).
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "feeds.h"

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

// RSS 2.0: CDATA, content:encoded, a guid, an enclosure, entities in a title, relative links.
static const char RSS[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<rss version=\"2.0\" xmlns:content=\"http://purl.org/rss/1.0/modules/content/\"\n"
    "     xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:media=\"http://search.yahoo.com/mrss/\">\n"
    "<channel>\n"
    "  <title>The Harbor  Gazette</title>\n"
    "  <link>https://harbor.example/</link>\n"
    "  <item>\n"
    "    <title>Tides &amp;amp; Times: the &lt;b&gt;new&lt;/b&gt; ferry&#8217;s first trip</title>\n"
    "    <link>/news/ferry</link>\n"
    "    <guid isPermaLink=\"false\">harbor-101</guid>\n"
    "    <dc:creator>Ana Example</dc:creator>\n"
    "    <pubDate>Tue, 10 Jun 2003 04:00:00 +0200</pubDate>\n"
    "    <description><![CDATA[<p>A short <b>summary</b>.</p>]]></description>\n"
    "    <content:encoded><![CDATA[<p>The whole story, with a <img src=\"/img/ferry.jpg\" alt=\"ferry\"> picture.</p>]]></content:encoded>\n"
    "  </item>\n"
    "  <item>\n"
    "    <title>Fog &eacute;t&eacute; warning</title>\n"
    "    <link>https://harbor.example/news/fog</link>\n"
    "    <author>desk@harbor.example (Lio Arden)</author>\n"
    "    <pubDate>Wed, 11 Jun 2003 09:30:00 GMT</pubDate>\n"
    "    <description>Fog &lt;i&gt;tonight&lt;/i&gt;.</description>\n"
    "    <enclosure url=\"https://cdn.example/fog.png\" type=\"image/png\" length=\"100\"/>\n"
    "  </item>\n"
    "  <item>\n"
    "    <title>No date</title>\n"
    "    <guid>https://harbor.example/news/nodate</guid>\n"
    "    <media:thumbnail url=\"thumb/nodate.jpg\"/>\n"
    "  </item>\n"
    "  <item></item>\n"
    "</channel>\n"
    "</rss>\n";

// Atom: xhtml content, a summary only, links by rel, published and updated.
static const char ATOM[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
    "  <title type=\"text\">Lighthouse Notes</title>\n"
    "  <link rel=\"self\" href=\"https://notes.example/atom.xml\"/>\n"
    "  <link href=\"https://notes.example/\"/>\n"
    "  <entry>\n"
    "    <title>Keeping the lamp</title>\n"
    "    <id>tag:notes.example,2026:1</id>\n"
    "    <link rel=\"alternate\" type=\"text/html\" href=\"posts/lamp.html\"/>\n"
    "    <link rel=\"enclosure\" type=\"image/jpeg\" href=\"/pics/lamp.jpg\"/>\n"
    "    <updated>2026-11-20T10:00:00Z</updated>\n"
    "    <published>2026-11-19T22:15:30.250-03:00</published>\n"
    "    <author><name>Mara Fennick</name></author>\n"
    "    <content type=\"xhtml\"><div xmlns=\"http://www.w3.org/1999/xhtml\"><p>Oil <em>every</em> night.</p></div></content>\n"
    "  </entry>\n"
    "  <entry>\n"
    "    <title type=\"html\">Storms &amp;amp; stars</title>\n"
    "    <id>tag:notes.example,2026:2</id>\n"
    "    <link href=\"https://notes.example/posts/storms.html\"/>\n"
    "    <updated>2026-11-21</updated>\n"
    "    <summary>Wind &lt; 40 knots &amp; rain.</summary>\n"
    "  </entry>\n"
    "</feed>\n";

// RSS 1.0 (RDF): items beside the channel, dc:date.
static const char RDF[] =
    "<?xml version=\"1.0\"?>\n"
    "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns=\"http://purl.org/rss/1.0/\"\n"
    "         xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
    "  <channel rdf:about=\"https://bay.example/\"><title>Bay Log</title><link>https://bay.example/</link></channel>\n"
    "  <item rdf:about=\"https://bay.example/1\"><title>Gulls</title><link>https://bay.example/1</link>\n"
    "    <dc:date>2025-01-02T03:04:05+01:00</dc:date><description>Many gulls.</description></item>\n"
    "</rdf:RDF>\n";

// Latin-1, as some old sites still send it.
static const char LATIN1[] =
    "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
    "<rss version=\"2.0\"><channel><title>Caf\xE9 del puerto</title>\n"
    "<item><title>Ma\xF1" "ana</title><link>https://cafe.example/1</link></item></channel></rss>\n";

// Not well-formed: a bare & and a tag never closed.
static const char BROKEN[] =
    "<rss version=\"2.0\"><channel><title>Broken & Co</title>\n"
    "<item><title>Still here</title><guid>https://broken.example/1</guid><description>Text</item>\n"
    "</channel></rss>";

static const char PAGE[] =
    "<!DOCTYPE html><html><head><title>Harbor</title>\n"
    "<link rel=\"stylesheet\" href=\"/s.css\">\n"
    "<link rel=\"alternate\" type=\"application/rss+xml\" title=\"RSS\" href=\"/feed.xml\">\n"
    "</head><body><p>Hello</body></html>";

static const char OPML[] =
    "<?xml version=\"1.0\"?><opml version=\"2.0\"><head><title>Mine</title></head><body>\n"
    "<outline text=\"News\"><outline text=\"Harbor\" title=\"Harbor Gazette\" type=\"rss\" "
    "xmlUrl=\"https://harbor.example/rss\"/>\n"
    "<outline text=\"Notes &amp; more\" xmlUrl=\"https://notes.example/atom.xml\"/></outline>\n"
    "<outline text=\"No url\"/><outline text=\"Bad\" xmlUrl=\"javascript:x\"/></body></opml>";

static void testRss(void)
{
    Feed f;
    CHECK(feedsParse(RSS, sizeof(RSS) - 1, "https://harbor.example/rss.xml", &f));
    CHECK_STR(f.title, "The Harbor Gazette");
    CHECK_STR(f.link, "https://harbor.example/");
    CHECK(f.count == 3);  // The empty item isn't an article.
    if (f.count == 3) {
        FeedItem *a = &f.items[0];
        CHECK_STR(a->title, "Tides & Times: the new ferry\xE2\x80\x99s first trip");
        CHECK_STR(a->link, "https://harbor.example/news/ferry");
        CHECK_STR(a->id, "harbor-101");
        CHECK_STR(a->author, "Ana Example");
        CHECK(a->date == 1055210400);  // 2003-06-10 02:00 UTC.
        CHECK(a->html && strstr(a->html, "The whole story"));  // content:encoded over description.
        CHECK_STR(a->image, "https://harbor.example/img/ferry.jpg");  // Its first picture.
        FeedItem *b = &f.items[1];
        CHECK_STR(b->title, "Fog \xC3\xA9t\xC3\xA9 warning");
        CHECK_STR(b->author, "Lio Arden");
        CHECK_STR(b->image, "https://cdn.example/fog.png");
        CHECK(b->html && strstr(b->html, "<i>tonight</i>"));
        CHECK_STR(b->id, "https://harbor.example/news/fog");  // No guid: its link.
        FeedItem *c = &f.items[2];
        CHECK(c->date == 0);
        CHECK_STR(c->link, "https://harbor.example/news/nodate");  // A permalink guid.
        CHECK_STR(c->image, "https://harbor.example/thumb/nodate.jpg");
        CHECK(c->html == NULL);
    }
    feedsFree(&f);
    CHECK(f.count == 0 && f.items == NULL);
}

static void testAtom(void)
{
    Feed f;
    CHECK(feedsParse(ATOM, sizeof(ATOM) - 1, "https://notes.example/atom.xml", &f));
    CHECK_STR(f.title, "Lighthouse Notes");
    CHECK_STR(f.link, "https://notes.example/");
    CHECK(f.count == 2);
    if (f.count == 2) {
        FeedItem *a = &f.items[0];
        CHECK_STR(a->link, "https://notes.example/posts/lamp.html");
        CHECK_STR(a->image, "https://notes.example/pics/lamp.jpg");
        CHECK_STR(a->author, "Mara Fennick");
        CHECK(a->date == 1795137330);  // Published: 2026-11-20 01:15:30 UTC.
        CHECK(a->html && strstr(a->html, "<p>Oil <em>every</em> night.</p>"));
        FeedItem *b = &f.items[1];
        CHECK_STR(b->title, "Storms & stars");
        CHECK(b->date == 1795219200);  // A date only: its midnight, UTC.
        CHECK(b->html && strstr(b->html, "Wind &lt; 40 knots &amp; rain."));  // Plain text, escaped.
    }
    feedsFree(&f);
}

static void testRdfLatinBroken(void)
{
    Feed f;
    CHECK(feedsParse(RDF, sizeof(RDF) - 1, "https://bay.example/rss", &f));
    CHECK_STR(f.title, "Bay Log");
    CHECK(f.count == 1 && !strcmp(f.items[0].title, "Gulls") && f.items[0].date == 1735783445);
    CHECK(f.count == 1 && !strcmp(f.items[0].id, "https://bay.example/1"));
    feedsFree(&f);

    CHECK(feedsParse(LATIN1, sizeof(LATIN1) - 1, "https://cafe.example/rss", &f));
    CHECK_STR(f.title, "Caf\xC3\xA9 del puerto");
    CHECK(f.count == 1 && !strcmp(f.items[0].title, "Ma\xC3\xB1" "ana"));
    feedsFree(&f);

    // Broken: whatever can be read, and never a crash.
    if (feedsParse(BROKEN, sizeof(BROKEN) - 1, "https://broken.example/", &f)) {
        CHECK(f.count <= 1);
        if (f.count == 1)
            CHECK_STR(f.items[0].title, "Still here");
        feedsFree(&f);
    }
    // Not feeds at all.
    CHECK(!feedsParse(PAGE, sizeof(PAGE) - 1, "https://harbor.example/", &f) && f.count == 0);
    CHECK(!feedsParse("", 0, "https://x.example/", &f));
    CHECK(!feedsParse("\x01\x02garbage<<<", 12, "https://x.example/", &f));
    static const char NOT_FEED[] = "<?xml version=\"1.0\"?><note><to>x</to></note>";
    CHECK(!feedsParse(NOT_FEED, sizeof(NOT_FEED) - 1, "https://x.example/", &f));
    // Every prefix of a feed (a download cut short) parses or doesn't, safely.
    for (size_t n = 1; n < sizeof(RSS) - 1; n += 7)
        if (feedsParse(RSS, n, "https://harbor.example/", &f))
            feedsFree(&f);
}

static void testDates(void)
{
    CHECK(feedsParseDate("Tue, 10 Jun 2003 04:00:00 GMT") == 1055217600);
    CHECK(feedsParseDate("10 Jun 2003 04:00 GMT") == 1055217600);
    CHECK(feedsParseDate("Tue, 10 Jun 03 00:00:00 EDT") == 1055217600);
    CHECK(feedsParseDate("Tue, 10 June 2003 06:00:00 +0200") == 1055217600);
    CHECK(feedsParseDate("2003-06-10T04:00:00Z") == 1055217600);
    CHECK(feedsParseDate("2003-06-10T06:00:00+02:00") == 1055217600);
    CHECK(feedsParseDate("2003-06-10 04:00:00") == 1055217600);
    CHECK(feedsParseDate("  2003-06-10T01:00:00.123-0300") == 1055217600);
    CHECK(feedsParseDate("Tue, 10 Jun 2003 04:00:00 XYZ") == 1055217600);  // An unknown zone: as UTC.
    CHECK(feedsParseDate("2003-02-30T00:00:00Z") == 0);
    CHECK(feedsParseDate("yesterday") == 0);
    CHECK(feedsParseDate("") == 0);
    CHECK(feedsParseDate(NULL) == 0);
    CHECK(feedsParseDate("Tue, 99 Jun 2003") == 0);
    CHECK(feedsParseDate("1900-01-01") == 0);
    char text[16];
    feedsDateText(1055217600, text, sizeof(text));
    CHECK_STR(text, "2003-06-10");
    feedsDateText(0, text, sizeof(text));
    CHECK_STR(text, "");
    char when[24];
    feedsDateTimeText(1055217600 + 5 * 60, when, sizeof(when));
    CHECK_STR(when, "2003-06-10 04:05");
    CHECK(feedsParseDate(when) == 1055217600 + 5 * 60);
    feedsDateTimeText(0, when, sizeof(when));
    CHECK_STR(when, "");
}

static void testPlain(void)
{
    char out[32];
    feedsPlainText("  a <b>bold</b>\n\n  &amp;&nbsp;&#233;&#xE9; &bogus; < 3 ", out, sizeof(out));
    CHECK_STR(out, "a bold & \xC3\xA9\xC3\xA9 &bogus; < 3");
    char small[6];
    feedsPlainText("ab\xC3\xA9\xC3\xA9", small, sizeof(small));  // Cut at a whole character.
    CHECK_STR(small, "ab\xC3\xA9");
    feedsPlainText("&#0;&#xD800;&#1114112;", out, sizeof(out));  // Not characters: left as written.
    CHECK_STR(out, "&#0;&#xD800;&#1114112;");
    feedsPlainText(NULL, out, sizeof(out));
    CHECK_STR(out, "");
}

static void testDiscoverOpml(void)
{
    char url[256];
    CHECK(feedsDiscover(PAGE, sizeof(PAGE) - 1, "http://192.168.0.168:8099/page.html", url, sizeof(url)));
    CHECK_STR(url, "http://192.168.0.168:8099/feed.xml");
    static const char NONE[] = "<html><head><link rel=\"alternate\" hreflang=\"es\" href=\"/es/\"></head></html>";
    CHECK(!feedsDiscover(NONE, sizeof(NONE) - 1, "https://x.example/", url, sizeof(url)));
    CHECK(!feedsDiscover("", 0, "https://x.example/", url, sizeof(url)));

    FeedOutline *o;
    int n = feedsParseOpml(OPML, sizeof(OPML) - 1, &o);
    CHECK(n == 2);
    if (n == 2) {
        CHECK_STR(o[0].url, "https://harbor.example/rss");
        CHECK_STR(o[0].title, "Harbor Gazette");
        CHECK_STR(o[1].title, "Notes & more");
    }
    free(o);
    CHECK(feedsParseOpml(RSS, sizeof(RSS) - 1, &o) == -1 && o == NULL);
}

static void addItem(Feed *f, const char *id, int64_t date)
{
    f->items = realloc(f->items, (size_t)(f->count + 1) * sizeof(*f->items));
    FeedItem *it = &f->items[f->count++];
    memset(it, 0, sizeof(*it));
    snprintf(it->id, sizeof(it->id), "%s", id);
    snprintf(it->title, sizeof(it->title), "Title %s", id);
    it->date = date;
    it->html = strdup(id);
}

static void testMergeWrite(void)
{
    Feed kept = {0}, fresh = {0};
    addItem(&kept, "a", 100);
    addItem(&kept, "b", 300);
    addItem(&kept, "z", 0);
    addItem(&fresh, "b", 350);  // Newer words for b.
    addItem(&fresh, "c", 200);
    addItem(&fresh, "c", 999);  // The same id twice in a feed: the first.
    addItem(&fresh, "y", 0);
    snprintf(fresh.title, sizeof(fresh.title), "%s", "Fresh");
    CHECK(feedsMerge(&kept, &fresh, 4));
    CHECK(fresh.count == 0 && fresh.items == NULL);
    CHECK(kept.count == 4);
    if (kept.count == 4) {
        CHECK(!strcmp(kept.items[0].id, "b") && kept.items[0].date == 350);
        CHECK(!strcmp(kept.items[1].id, "c") && kept.items[1].date == 200);
        CHECK_STR(kept.items[2].id, "a");
        CHECK_STR(kept.items[3].id, "y");  // Undated last, the fresh one first; z didn't fit.
    }
    CHECK_STR(kept.title, "Fresh");

    // Written for the card and read back the same.
    snprintf(kept.link, sizeof(kept.link), "%s", "https://site.example/");
    snprintf(kept.items[0].author, sizeof(kept.items[0].author), "%s", "Ana & \"Co\"");
    snprintf(kept.items[0].image, sizeof(kept.items[0].image), "%s", "https://site.example/p.jpg?a=1&b=2");
    free(kept.items[0].html);
    kept.items[0].html = strdup("<p>One &amp; <b>two</b> \x01</p>");
    size_t size;
    char *xml = feedsWrite(&kept, &size);
    CHECK(xml && size == strlen(xml));
    Feed back;
    CHECK(xml && feedsParse(xml, size, "file:///kept", &back));
    CHECK(back.count == 4);
    if (back.count == 4) {
        CHECK_STR(back.title, "Fresh");
        CHECK_STR(back.link, "https://site.example/");
        CHECK(!strcmp(back.items[0].id, "b") && back.items[0].date == 350);
        CHECK_STR(back.items[0].author, "Ana & \"Co\"");
        CHECK_STR(back.items[0].image, "https://site.example/p.jpg?a=1&b=2");
        CHECK(back.items[0].html && !strcmp(back.items[0].html, "<p>One &amp; <b>two</b> </p>"));
        CHECK(back.items[3].date == 0);
    }
    feedsFree(&back);
    free(xml);
    feedsFree(&kept);
    Feed empty = {0}, none = {0};
    CHECK(feedsMerge(&empty, &none, FEED_KEEP) && empty.count == 0);
    xml = feedsWrite(&empty, &size);
    CHECK(xml && feedsParse(xml, size, "", &back) && back.count == 0);
    feedsFree(&back);
    free(xml);
}

static void testHash(void)
{
    char a[FEED_HASH_LEN + 1], b[FEED_HASH_LEN + 1];
    feedsHash("https://harbor.example/rss", a);
    feedsHash("https://harbor.example/rss2", b);
    CHECK(strlen(a) == FEED_HASH_LEN && strcmp(a, b) != 0);
    feedsHash("", b);
    CHECK_STR(b, "cbf29ce484222325");
}

void testNewsFeeds(void)
{
    testRss();
    testAtom();
    testRdfLatinBroken();
    testDates();
    testPlain();
    testDiscoverOpml();
    testMergeWrite();
    testHash();
}
