// The news' network work (newsjob.c, with net.c's curl) against the made-up site of
// make_server.py, served on the PC: finding a feed from a page, checking two feeds (kept
// in ./kept, then read back), and an article's whole page. Built and run by live.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "feedlist.h"
#include "net.h"
#include "newsjob.h"

static int g_failures;
#define EXPECT(c)                                                        \
    do {                                                                 \
        if (!(c)) {                                                      \
            g_failures++;                                                \
            printf("FAILED line %d: %s\n", __LINE__, #c);                \
        }                                                                \
    } while (0)

static void nap(void)
{
    struct timespec ts = {0, 20000000};
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv)
{
    const char *site = argc > 1 ? argv[1] : "http://host.docker.internal:8099";
    char url[512], found[FEED_URL_MAX], title[FEED_TEXT_MAX], why[NETJOB_WHY_MAX];
    bool ok;
    if (!netInit())
        return 1;
    mkdir("kept", 0755);

    // A page that links to its feed: the feed found.
    snprintf(url, sizeof(url), "%s/page.html", site);
    EXPECT(newsJobFind(url));
    while (!newsJobTakeFind(found, sizeof(found), title, sizeof(title), &ok, why, sizeof(why)))
        nap();
    printf("find %s -> %s \"%s\" %s\n", url, found, title, ok ? "ok" : why);
    EXPECT(ok && strstr(found, "/rss.xml") && !strcmp(title, "The Harbor Gazette (test)"));
    snprintf(url, sizeof(url), "%s/nothing.html", site);
    EXPECT(newsJobFind(url));
    while (!newsJobTakeFind(found, sizeof(found), title, sizeof(title), &ok, why, sizeof(why)))
        nap();
    printf("find %s -> %s\n", url, ok ? "ok?" : why);
    EXPECT(!ok);

    // Both feeds checked: what's kept first (nothing), then what the site says.
    NewsJobFeed feeds[3] = {0};
    snprintf(feeds[0].url, sizeof(feeds[0].url), "%s/rss.xml", site);
    snprintf(feeds[1].url, sizeof(feeds[1].url), "%s/atom.xml", site);
    snprintf(feeds[2].url, sizeof(feeds[2].url), "%s/missing.xml", site);
    for (int i = 0; i < 3; i++)
        feedsHash(feeds[i].url, feeds[i].hash);
    for (int pass = 0; pass < 2; pass++) {
        EXPECT(newsJobCheck(feeds, 3, "kept"));
        NewsArticle *list;
        NewsFeedState *states;
        int count;
        bool asked = false;
        for (;;) {
            int state_count;
            while (!newsJobTakeCheck(&list, &count, &states, &state_count, &asked))
                nap();
            printf("pass %d round (asked %d): %d articles\n", pass, asked, count);
            if (asked)
                break;
            EXPECT(count == (pass ? 5 : 0));  // The second time, what was kept comes first.
            free(list);
            free(states);
        }
        for (int i = 0; i < count; i++)
            printf("  %s  %lld  %s  [%s]\n", list[i].path, (long long)list[i].date, list[i].title, list[i].image);
        for (int i = 0; i < 3; i++)
            printf("  feed %d: %s \"%s\" %s\n", i, states[i].ok ? "ok" : "failed", states[i].title, states[i].why);
        EXPECT(count == 5 && states[0].ok && states[1].ok && !states[2].ok);
        bool ferry = false;
        for (int i = 0; i < count; i++)
            ferry |= !strcmp(list[i].title, "New ferry makes its first crossing") &&
                     strstr(list[i].image, "/pictures/ferry.png") != NULL;
        EXPECT(ferry && count > 1 && list[0].date >= list[1].date);
        free(list);
        free(states);
    }

    // The whole article from its page.
    snprintf(url, sizeof(url), "%s/article.html", site);
    EXPECT(newsJobFull(url, false));
    char *html;
    while (!newsJobTakeFull(&html, &ok, why, sizeof(why)))
        nap();
    printf("full: %s\n%s\n", ok ? "ok" : why, html ? html : "");
    EXPECT(ok && html && strstr(html, "twice a day") && !strstr(html, "Sport") && !strstr(html, "tracking"));
    free(html);
    // An article's pictures: one kept as a JPEG, one that isn't there marked so.
    char urls[2][FEED_URL_MAX], files[2][NEWSJOB_PICTURE_PATH];
    snprintf(urls[0], sizeof(urls[0]), "%s/pictures/ferry.png", site);
    snprintf(urls[1], sizeof(urls[1]), "%s/pictures/missing.png", site);
    snprintf(files[0], sizeof(files[0]), "%s", "kept/ferry.jpg");
    snprintf(files[1], sizeof(files[1]), "%s", "kept/missing.jpg");
    remove(files[0]);
    remove("kept/missing.jpg.none");
    EXPECT(newsJobPictures((const char(*)[FEED_URL_MAX])urls, (const char(*)[NEWSJOB_PICTURE_PATH])files, 2, false, 640));
    int kept = -1;
    while (!newsJobTakePictures(&kept))
        nap();
    FILE *jpg = fopen(files[0], "rb");
    FILE *none = fopen("kept/missing.jpg.none", "rb");
    printf("pictures: %d kept, ferry %s, missing marked %s\n", kept, jpg ? "saved" : "NOT saved", none ? "yes" : "no");
    EXPECT(kept == 1 && jpg && none);
    if (jpg)
        fclose(jpg);
    if (none)
        fclose(none);
    newsJobStop();
    netExit();
    printf("%d failures\n", g_failures);
    return g_failures ? 1 : 0;
}
