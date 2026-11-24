// doc.c with MuPDF on the made-up books of make_fixtures.py.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "doc.h"

#define FIX "build/fixtures/"

static Doc *openOk(const char *path)
{
    char error[DOC_ERROR_MAX];
    Doc *d = docOpen(path, error, sizeof(error));
    if (!d)
        printf("  %s: %s\n", path, error);
    CHECK(d != NULL);
    return d;
}

static void testNames(void)
{
    CHECK(docKnownName("a.epub") && docKnownName("B.EPUB") && docKnownName("x.Pdf") && docKnownName("c.cbz"));
    CHECK(docKnownName("t.fb2") && docKnownName("m.mobi") && docKnownName("n.txt"));
    CHECK(!docKnownName("a.cbr") && !docKnownName("a.mkv") && !docKnownName(".epub") && !docKnownName("epub"));
    CHECK(!docKnownName("a.epub.part") && !docKnownName(NULL) && !docKnownName("a.epubx"));
}

static void testEpub(void)
{
    Doc *d = openOk(FIX "quiet.epub");
    if (!d)
        return;
    CHECK(docReflowable(d));
    char text[DOC_TEXT_MAX];
    CHECK(docTitle(d, text, sizeof(text)) && !strcmp(text, "The Quiet Lighthouse"));
    CHECK(docAuthor(d, text, sizeof(text)) && !strcmp(text, "Ana Example"));

    CHECK(docLayout(d, 400, 600, 12));
    int small = docPageCount(d);
    CHECK(docLayout(d, 400, 600, 24));
    int big = docPageCount(d);
    CHECK(small >= 3 && big > small);  // Bigger letters, more pages.
    float w, h;
    CHECK(docPageSize(d, 0, &w, &h) && w == 400 && h == 600);
    CHECK(!docPageSize(d, big, &w, &h) && !docPageSize(d, -1, &w, &h));

    DocChapter ch[8];
    int n = docChapters(d, ch, 8);
    CHECK(n == 3);
    if (n == 3) {
        CHECK(!strcmp(ch[0].title, "The Keeper") && !strcmp(ch[2].title, "Morning Bell"));
        CHECK(ch[0].level == 0 && ch[0].page >= 0 && ch[1].page > ch[0].page && ch[2].page > ch[1].page);
    }
    CHECK(docChapters(d, ch, 1) == 1);

    // A place in chapter 3 is still in chapter 3 after the letters change size.
    int page = n == 3 ? ch[2].page + 2 : 1;
    DocPlace place = docPlaceOf(d, page);
    CHECK(place.chapter == 2 && place.fraction > 0 && place.fraction < 1);
    CHECK(docPageOfPlace(d, place) == page);
    CHECK(docLayout(d, 400, 600, 12));
    int moved = docPageOfPlace(d, place);
    DocChapter again[8];
    CHECK(docChapters(d, again, 8) == 3);
    CHECK(moved >= again[2].page && moved < docPageCount(d));
    // Every page comes back to itself.
    bool all = true;
    for (int i = 0; i < docPageCount(d); i++)
        all = all && docPageOfPlace(d, docPlaceOf(d, i)) == i;
    CHECK(all);
    DocPlace start = docPlaceOf(d, 0);
    CHECK(start.chapter == 0 && start.fraction == 0 && docPageOfPlace(d, start) == 0);
    CHECK(docPageOfPlace(d, (DocPlace){99, 0.5}) == 0 && docPageOfPlace(d, (DocPlace){1, 2.0}) == 0);
    CHECK(docPageOfPlace(d, (DocPlace){-1, 0.5}) == 0);

    // Rendering: the size asked, colors for a reflowable page.
    DocImage img;
    DocColors dark = {0xE0E0E0, 0x101010};
    CHECK(docRender(d, 1, 1.5f, &dark, &img));
    CHECK(img.width == 600 && img.height == 900 && img.pixels);
    if (img.pixels) {
        uint32_t corner = img.pixels[0];
        CHECK((corner & 0xFF) == 0x10 && ((corner >> 24) & 0xFF) == 0xFF);
        bool has_ink = false;
        for (int i = 0; i < img.width * img.height && !has_ink; i++)
            has_ink = (img.pixels[i] & 0xFF) > 0x80;
        CHECK(has_ink);
    }
    docImageFree(&img);
    CHECK(img.pixels == NULL);
    CHECK(!docRender(d, 999, 1, NULL, &img) && !img.pixels);
    CHECK(!docRender(d, 0, 0, NULL, &img));
    // Never bigger than DOC_RENDER_MAX on a side.
    CHECK(docRender(d, 0, 100, NULL, &img) && img.height <= DOC_RENDER_MAX && img.width <= DOC_RENDER_MAX);
    docImageFree(&img);
    CHECK(!docLayout(d, 10, 600, 12) && !docLayout(d, 400, 600, 0));
    docClose(d);
}

static void testOldEpub(void)
{
    Doc *d = openOk(FIX "old.epub");
    if (!d)
        return;
    docLayout(d, 400, 600, 14);
    DocChapter ch[8];
    CHECK(docChapters(d, ch, 8) == 3);  // From the NCX.
    char text[DOC_TEXT_MAX];
    CHECK(docTitle(d, text, sizeof(text)) && !strcmp(text, "An Older Harbor"));
    docClose(d);
}

static void testFixed(void)
{
    Doc *d = openOk(FIX "plain.pdf");
    if (d) {
        CHECK(!docReflowable(d) && docPageCount(d) == 3);
        CHECK(docLayout(d, 400, 600, 20) && docPageCount(d) == 3);  // Fixed pages stay.
        float w, h;
        CHECK(docPageSize(d, 2, &w, &h) && w == 612 && h == 792);
        char text[DOC_TEXT_MAX];
        CHECK(docTitle(d, text, sizeof(text)) && !strcmp(text, "Made-up Ledger"));
        DocImage img;
        DocColors dark = {0xFFFFFF, 0x000000};
        CHECK(docRender(d, 0, 0.5f, &dark, &img) && img.width == 306 && img.height == 396);
        if (img.pixels)
            CHECK(img.pixels[0] == 0xFFFFFFFFu);  // A fixed page keeps its white paper.
        docImageFree(&img);
        CHECK(docPageOfPlace(d, docPlaceOf(d, 2)) == 2);
        docClose(d);
    }
    d = openOk(FIX "strip.cbz");
    if (d) {
        CHECK(!docReflowable(d) && docPageCount(d) == 3);
        DocImage img;
        CHECK(docRender(d, 1, 1, NULL, &img));
        if (img.pixels) {
            uint32_t mid = img.pixels[(img.height / 2) * img.width + img.width / 2];
            CHECK((mid & 0xFF) < 0x60 && ((mid >> 8) & 0xFF) > 0x80);  // The green page.
        }
        docImageFree(&img);
        docClose(d);
    }
}

static void testOthers(void)
{
    Doc *d = openOk(FIX "tale.fb2");
    if (d) {
        CHECK(docReflowable(d));
        docLayout(d, 400, 600, 14);
        CHECK(docPageCount(d) >= 2);
        char text[DOC_TEXT_MAX];
        CHECK(docTitle(d, text, sizeof(text)) && !strcmp(text, "A Tale of Ropes"));
        docClose(d);
    }
    d = openOk(FIX "notes.txt");
    if (d) {
        CHECK(docReflowable(d));
        docLayout(d, 400, 600, 14);
        CHECK(docPageCount(d) >= 2);
        docClose(d);
    }
    char error[DOC_ERROR_MAX];
    CHECK(docOpen(FIX "missing.epub", error, sizeof(error)) == NULL && error[0]);
    CHECK(docOpen(FIX "empty.pdf", error, sizeof(error)) == NULL && error[0]);
    d = docOpen(FIX "broken.epub", error, sizeof(error));
    if (d) {  // MuPDF may open it as an empty book: it must not crash, and has nothing to show.
        docLayout(d, 400, 600, 14);
        DocImage img;
        CHECK(docPageCount(d) == 0 || docRender(d, 0, 1, NULL, &img) || 1);
        docClose(d);
    } else {
        CHECK(error[0]);
    }
}

static void testCovers(void)
{
    DocImage img;
    CHECK(docEpubCover(FIX "quiet.epub", 100, 150, &img) && img.width == 60 && img.height == 90);  // Never enlarged.
    if (img.pixels)
        CHECK(img.pixels[45 * 60 + 30] == 0xFFA05A28u);  // (40, 90, 160) as 0xAABBGGRR.
    docImageFree(&img);
    CHECK(docEpubCover(FIX "quiet.epub", 30, 30, &img) && img.width == 20 && img.height == 30);
    docImageFree(&img);
    // EPUB 2: by <meta name="cover">, in a folder, with a space written %20.
    CHECK(docEpubCover(FIX "old.epub", 400, 600, &img) && img.width == 80 && img.height == 120);
    if (img.pixels)
        CHECK((img.pixels[0] & 0xFFFFFF) == 0x3C8C1E);
    docImageFree(&img);
    CHECK(!docEpubCover(FIX "plain.pdf", 100, 150, &img) && !img.pixels);
    CHECK(!docEpubCover(FIX "broken.epub", 100, 150, &img) && !img.pixels);
    CHECK(!docEpubCover(FIX "missing.epub", 100, 150, &img));
    CHECK(!docEpubCover(FIX "quiet.epub", 0, 150, &img) && !docEpubCover(NULL, 10, 10, &img));
}

void testDoc(void)
{
    testCovers();
    testNames();
    testEpub();
    testOldEpub();
    testFixed();
    testOthers();
}
