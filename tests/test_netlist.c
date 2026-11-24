// netlist.c: what of the network folder shows, and its books' names on the card.
#include <string.h>

#include "check.h"
#include "netlist.h"

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static void testListing(void)
{
    DavEntry e[] = {
        {"zeta.epub", false, 10, 0}, {"Notes", true, -1, 0}, {"movie.mkv", false, 10, 0},
        {".hidden.epub", false, 10, 0}, {"alpha.PDF", false, 10, 0}, {"comics", true, -1, 0},
        {".git", true, -1, 0}, {"Beta.cbr", false, 10, 0},
    };
    size_t n = netListBooks(e, sizeof(e) / sizeof(e[0]));
    CHECK(n == 5);
    if (n == 5) {
        CHECK_STR(e[0].name, "comics");
        CHECK_STR(e[1].name, "Notes");
        CHECK_STR(e[2].name, "alpha.PDF");
        CHECK_STR(e[3].name, "Beta.cbr");
        CHECK_STR(e[4].name, "zeta.epub");
    }
    CHECK(netListBooks(e, 0) == 0);
}

static void testNames(void)
{
    char out[128];
    netPlainName("Café au lait.epub", false, out, sizeof(out));
    CHECK_STR(out, "Cafe au lait.epub");
    netPlainName("Part 1: The Sea?.PDF", false, out, sizeof(out));
    CHECK_STR(out, "Part 1 - The Sea.pdf");
    netPlainName(".hidden", true, out, sizeof(out));
    CHECK_STR(out, "hidden");
    netPlainName("v1.0 notes", true, out, sizeof(out));  // A folder: its dot isn't an extension.
    CHECK_STR(out, "v1.0 notes");
    // Left out characters: a code of the whole name, so two of them stay two.
    char a[128], b[128];
    netPlainName("\xE6\x9D\xB1\xE4\xBA\xAC.epub", false, a, sizeof(a));      // 東京.epub
    netPlainName("\xE5\xA4\xA7\xE9\x98\xAA.epub", false, b, sizeof(b));      // 大阪.epub
    CHECK(a[0] == '~' && strlen(a) == strlen("~123456.epub") && strstr(a, ".epub"));
    CHECK(strcmp(a, b) != 0);
    netPlainName("Tokyo \xE6\x9D\xB1\xE4\xBA\xAC.cbz", false, out, sizeof(out));
    CHECK(!strncmp(out, "Tokyo ~", 7) && strstr(out, ".cbz"));
    for (const char *c = out; *c; c++)
        CHECK((unsigned char)*c < 0x80);
    char tiny[12];  // Cut to fit, the extension kept.
    netPlainName("A very long name of a book.epub", false, tiny, sizeof(tiny));
    CHECK(strlen(tiny) < sizeof(tiny) && strstr(tiny, ".epub"));
}

static void testPaths(void)
{
    char out[256];
    CHECK(netLocalPath("sdmc:/books", "dav:/Caf\xC3\xA9/Mon livre.epub", out, sizeof(out)));
    CHECK_STR(out, "sdmc:/books/.network/Cafe/Mon livre.epub");
    CHECK(!netLocalPath("sdmc:/books", "dav:/", out, sizeof(out)));
    CHECK(!netLocalPath("sdmc:/books", "dav:/a/../b.epub", out, sizeof(out)));
    CHECK(!netLocalPath("sdmc:/books", "/books/a.epub", out, sizeof(out)));
    CHECK(!netLocalPath("sdmc:/books", "dav:/a.epub", out, 20));

    CHECK(netParent("dav:/A/b.epub", out, sizeof(out)));
    CHECK_STR(out, "dav:/A");
    CHECK(netParent("dav:/A", out, sizeof(out)));
    CHECK_STR(out, "dav:/");
    CHECK(!netParent("dav:/", out, sizeof(out)));
    CHECK(netChild("dav:/", "A", out, sizeof(out)));
    CHECK_STR(out, "dav:/A");
    CHECK(netChild("dav:/A", "b.epub", out, sizeof(out)));
    CHECK_STR(out, "dav:/A/b.epub");
    CHECK_STR(netLastName("dav:/A/b.epub"), "b.epub");
    CHECK_STR(netLastName("dav:/"), "");
}

void testNetList(void)
{
    testListing();
    testNames();
    testPaths();
}
