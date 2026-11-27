// Host tests for the modules that do not touch Switch services.
// Build and run with: make -C tests run
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "util.h"

int g_failures;
int g_checks;

static void testUtil(void)
{
    char out[16];
    utilFormatClock(out, sizeof(out), 21, 7, true);
    CHECK(strcmp(out, "21:07") == 0);
    CHECK(utilCompareNames("abc", "ABD") < 0);
    const char *s = "\xC3\xA9x";
    CHECK(utilNextCodePoint(&s) == 0xE9 && *s == 'x');
}

int main(void)
{
    testUtil();
    testAppRoot();
    testKinetic();
    testLang();
    testNotice();
    testDoc();
    testShelf();
    testReading();
    testPrefs();
    testSettings();
    testBookmarks();
    testDav();
    testFolderList();
    testNetList();
    testTouch();
    testOpds();
    testNewsFeeds();
    testNews();
    testCoverCache();
    testCatalogs();
    testUpdate();
    testUpdateRelease();
    testCopyBar();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
