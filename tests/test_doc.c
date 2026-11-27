// doc.c with MuPDF on the made-up books of make_fixtures.py.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mupdf/fitz.h>

#include "check.h"
#include "doc.h"

#define FIX "build/fixtures/"

static Doc *openOk(const char *path)
{
    char error[DOC_ERROR_MAX];
    Doc *d = docOpen(path, NULL, error, sizeof(error));
    if (!d)
        printf("  %s: %s\n", path, error);
    CHECK(d != NULL);
    return d;
}

static void testNames(void)
{
    CHECK(docKnownName("a.epub") && docKnownName("B.EPUB") && docKnownName("x.Pdf") && docKnownName("c.cbz"));
    CHECK(docKnownName("t.fb2") && docKnownName("m.mobi") && docKnownName("n.txt"));
    CHECK(!docKnownName("a.cbx") && !docKnownName("a.mkv") && !docKnownName(".epub") && !docKnownName("epub"));
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

    // Bigger letters keep the reader's place exactly: the text that began the
    // page read is on the page shown after (not a page before or after it).
    CHECK(docLayout(d, 400, 600, 12));
    for (int p = 1; p + 1 < docPageCount(d); p += 3) {
        char start[40], after[4096];
        CHECK(docPageText(d, p, start, sizeof(start)) && start[0]);
        char *cut = strrchr(start, ' ');  // Whole words only.
        if (cut)
            *cut = '\0';
        int kept = p;
        CHECK(docLayoutKeeping(d, 400, 600, 24, &kept));
        CHECK(docPageText(d, kept, after, sizeof(after)) && strstr(after, start) != NULL);
        CHECK(docLayout(d, 400, 600, 12));
    }
    char none[8];
    CHECK(!docPageText(d, -1, none, sizeof(none)) && !none[0]);
    CHECK(docLayout(d, 400, 600, 24));

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
    docClose(d);
    char error[DOC_ERROR_MAX];
    d = docOpen(FIX "quiet.epub", &dark, error, sizeof(error));
    CHECK(d != NULL);
    if (!d)
        return;
    CHECK(docLayout(d, 400, 600, 24));
    CHECK(docRender(d, 1, 1.5f, &img));
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
    CHECK(!docRender(d, 999, 1, &img) && !img.pixels);
    CHECK(!docRender(d, 0, 0, &img));
    // Never bigger than DOC_RENDER_MAX on a side.
    CHECK(docRender(d, 0, 100, &img) && img.height <= DOC_RENDER_MAX && img.width <= DOC_RENDER_MAX);
    docImageFree(&img);
    CHECK(!docLayout(d, 10, 600, 12) && !docLayout(d, 400, 600, 0));
    docClose(d);
}

// Colors change the text and the paper, never a picture: a red one stays red.
static void testPictureColors(void)
{
    DocColors dark = {0xD0D0D0, 0x121212};
    char error[DOC_ERROR_MAX];
    Doc *d = docOpen(FIX "picture.epub", &dark, error, sizeof(error));
    CHECK(d != NULL);
    if (!d)
        return;
    CHECK(docLayout(d, 400, 600, 14));
    DocImage img;
    CHECK(docRender(d, 0, 1, &img));
    int red = 0, light_text = 0, black = 0;
    for (int i = 0; img.pixels && i < img.width * img.height; i++) {
        uint32_t p = img.pixels[i];
        int r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF;
        red += r > 200 && g < 50 && b < 50;
        light_text += r > 0xB0 && g > 0xB0 && b > 0xB0;
        black += r < 8 && g < 8 && b < 8;
    }
    CHECK(red > 3000);        // 120 x 60 pixels, at 96 per inch: about 90 x 45 points, red as it is.
    CHECK(light_text > 100);  // The book's black text is in the ink.
    CHECK(black == 0);        // Nothing of the book's own black, nor of the old dark tint, is left.
    if (img.pixels)
        CHECK((img.pixels[0] & 0xFFFFFF) == 0x121212);  // The paper.
    docImageFree(&img);
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

// The same three-page strip as a tar (CBT) and a 7z (CB7, through libarchive): its pages and their colors.
static void testComicArchives(void)
{
    static const char *const FILES[] = {FIX "strip-tar.cbt", FIX "strip-7z.cb7"};
    for (int f = 0; f < 2; f++) {
        Doc *d = openOk(FILES[f]);
        if (!d)
            continue;
        CHECK(!docReflowable(d) && docPageCount(d) == 3);
        DocImage img;
        CHECK(docRender(d, 1, 1.0f, &img) && img.width > 0);
        if (img.pixels)
            CHECK((img.pixels[img.width * (img.height / 2) + img.width / 2] & 0xFFFFFF) == 0x3CA028);  // Green, page 2.
        docImageFree(&img);
        docClose(d);
    }
    CHECK(docKnownName("x.cbr") && docKnownName("X.CB7"));
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
        CHECK(docRender(d, 0, 0.5f, &img) && img.width == 306 && img.height == 396);
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
        CHECK(docRender(d, 1, 1, &img));
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
    CHECK(docOpen(FIX "missing.epub", NULL, error, sizeof(error)) == NULL && error[0]);
    CHECK(docOpen(FIX "empty.pdf", NULL, error, sizeof(error)) == NULL && error[0]);
    d = docOpen(FIX "broken.epub", NULL, error, sizeof(error));
    if (d) {  // MuPDF may open it as an empty book: it must not crash, and has nothing to show.
        docLayout(d, 400, 600, 14);
        DocImage img;
        CHECK(docPageCount(d) == 0 || docRender(d, 0, 1, &img) || 1);
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

    // The genre and the author's sort name, EPUB 3 (a refining meta) and EPUB 2 (opf:file-as).
    DocEpubInfo info;
    CHECK(docEpubInfo(FIX "quiet.epub", 100, 150, &info) && info.cover.pixels);
    CHECK(!strcmp(info.genre, "Sea Stories") && !strcmp(info.author_sort, "Example, Ana"));
    docImageFree(&info.cover);
    CHECK(docEpubInfo(FIX "old.epub", 0, 0, &info) && !info.cover.pixels);  // Size 0: no picture asked.
    CHECK(!strcmp(info.genre, "Harbors") && !strcmp(info.author_sort, "Example, Ana"));
    CHECK(!docEpubInfo(FIX "plain.pdf", 10, 10, &info) && !info.genre[0]);
    CHECK(!docEpubInfo(FIX "broken.epub", 10, 10, &info) && !info.cover.pixels);
}

// Part of a page, for zoom past what a whole page can be: the same pixels as the whole one.
static void testRenderPart(void)
{
    Doc *d = openOk(FIX "plain.pdf");  // 612 x 792 points.
    if (!d)
        return;
    CHECK(docRenderScale(d, 0, 2.0f) == 2.0f);
    CHECK(docRenderScale(d, 0, 10.0f) < 10.0f && (int)(792 * docRenderScale(d, 0, 10.0f)) <= DOC_RENDER_MAX);
    DocImage whole, part;
    CHECK(docRender(d, 0, 2.0f, &whole) && docRenderPart(d, 0, 2.0f, 100, 150, 300, 200, &part));
    CHECK(part.width == 300 && part.height == 200);
    if (whole.pixels && part.pixels) {
        int differ = 0;
        for (int y = 0; y < part.height; y++)
            for (int x = 0; x < part.width; x++)
                differ += part.pixels[y * part.width + x] != whole.pixels[(y + 150) * whole.width + x + 100];
        CHECK(differ == 0);
    }
    docImageFree(&part);
    CHECK(docRenderPart(d, 0, 2.0f, whole.width - 50, 0, 300, 100, &part) && part.width == 50);  // Cut at the edge.
    docImageFree(&part);
    CHECK(!docRenderPart(d, 0, 2.0f, whole.width + 10, 0, 100, 100, &part) && !part.pixels);  // Off the page.
    CHECK(!docRenderPart(d, 0, 2.0f, 0, 0, DOC_RENDER_MAX + 1, 10, &part));
    // Far bigger than a whole page can be: still a part of it.
    CHECK(docRenderPart(d, 0, 10.0f, 3000, 4000, 640, 360, &part) && part.width == 640 && part.height == 360);
    docImageFree(&part);
    docImageFree(&whole);
    docClose(d);
}

// A "dav:/" path opened through the app's stream: here, a fixture file (as netstream.c reads one off the network).
static int g_remote_opens;

static fz_stream *openFixture(fz_context *ctx, const char *path)
{
    char file[256];
    snprintf(file, sizeof(file), FIX "%s", path + strlen(DOC_REMOTE_PREFIX));
    g_remote_opens++;
    return fz_open_file(ctx, file);
}

static void testRemote(void)
{
    char error[DOC_ERROR_MAX];
    CHECK(!docOpen("dav:/quiet.epub", NULL, error, sizeof(error)) && error[0]);  // No opener: it can't.
    docSetRemoteOpener(openFixture);
    Doc *d = docOpen("dav:/quiet.epub", NULL, error, sizeof(error));
    CHECK(d && g_remote_opens == 1 && docReflowable(d));
    if (d) {
        char title[DOC_TEXT_MAX];
        docTitle(d, title, sizeof(title));
        CHECK(!strcmp(title, "The Quiet Lighthouse"));
        docClose(d);
    }
    DocEpubInfo info;
    CHECK(docEpubInfo("dav:/quiet.epub", 100, 150, &info) && info.cover.pixels && g_remote_opens == 2);
    CHECK(!strcmp(info.title, "The Quiet Lighthouse") && !strcmp(info.author, "Ana Example"));  // A card from the package.
    docImageFree(&info.cover);
    d = docOpen("dav:/strip.cbz", NULL, error, sizeof(error));  // Its kind from its name.
    CHECK(d && !docReflowable(d) && docPageCount(d) == 3);
    if (d)
        docClose(d);
    d = docOpen("dav:/plain.pdf", NULL, error, sizeof(error));
    CHECK(d && docPageCount(d) == 3);
    if (d)
        docClose(d);
    CHECK(!docOpen("dav:/missing.epub", NULL, error, sizeof(error)) && error[0]);
    docSetRemoteOpener(NULL);
}

// A news article laid out as one tall page and cut in strips: the strips are rows of the same page,
// so where two meet the text goes on as it would with no cut (no line moved down a page).
static void testTallStrips(void)
{
    const char *path = TMP_ROOT "/tall.xhtml";
    FILE *f = fopen(path, "w");
    if (!f) {
        CHECK(f != NULL);
        return;
    }
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><body><h1>Tall</h1>", f);
    for (int i = 0; i < 60; i++)
        fprintf(f, "<p>Paragraph %d: the tide came in over the rocks and the gulls rose from the pier, again.</p>", i);
    fputs("</body></html>", f);
    fclose(f);
    char error[DOC_ERROR_MAX];
    DocColors colors = {0x000000, 0xFFFFFF};
    Doc *d = docOpen(path, &colors, error, sizeof(error));
    CHECK(d != NULL);
    if (!d)
        return;
    const float strip = 300;
    CHECK(!docLayoutTall(d, 400, DOC_TALL_MAX * 2, 14));
    CHECK(docLayoutTall(d, 400, strip * 400, 14) && docPageCount(d) == 1);
    float bottom = docTextBottom(d, 0);
    CHECK(bottom > strip * 3 && bottom < strip * 400);  // Several strips of text, and its end found.
    DocImage a, b, across;
    CHECK(docRenderStrip(d, 0, 1.0f, 0, (int)strip, &a) && docRenderStrip(d, 0, 1.0f, (int)strip, (int)strip, &b));
    CHECK(docRenderStrip(d, 0, 1.0f, (int)strip - 50, 100, &across));
    bool same = a.pixels && b.pixels && across.pixels && a.width == across.width && b.width == across.width &&
                a.height == (int)strip && across.height == 100;
    for (int y = 0; same && y < 100; y++) {
        const uint32_t *row = across.pixels + (size_t)y * across.width;
        const uint32_t *from = y < 50 ? a.pixels + (size_t)((int)strip - 50 + y) * a.width : b.pixels + (size_t)(y - 50) * b.width;
        same = memcmp(row, from, (size_t)across.width * sizeof(*row)) == 0;
    }
    CHECK(same);
    bool inked = false;  // And the join isn't between paragraphs only by luck: text runs over it somewhere.
    for (int s = 1; s < 6 && !inked; s++) {
        DocImage j;
        if (!docRenderStrip(d, 0, 1.0f, (int)(s * strip) - 2, 4, &j))
            break;
        for (int i = 0; i < j.width * j.height && !inked; i++)
            inked = (j.pixels[i] & 0xFF) < 0x80;
        docImageFree(&j);
    }
    CHECK(inked);
    docImageFree(&a);
    docImageFree(&b);
    docImageFree(&across);
    CHECK(!docRenderStrip(d, 0, 1.0f, 0, DOC_RENDER_MAX + 1, &across) && across.pixels == NULL);
    docClose(d);
}

// A picture kept as a JPEG: read back the same size, its colors near.
static void testSaveJpeg(void)
{
    DocImage image = {calloc(64 * 48, 4), 64, 48};
    if (!image.pixels)
        return;
    for (int i = 0; i < 64 * 48; i++)
        image.pixels[i] = 0xFF2060C0u;  // Blue-ish (0xAABBGGRR).
    const char *path = TMP_ROOT "/picture.jpg";
    CHECK(docImageSaveJpeg(&image, path, 82));
    FILE *f = fopen(path, "rb");
    unsigned char data[65536];
    size_t n = f ? fread(data, 1, sizeof(data), f) : 0;
    if (f)
        fclose(f);
    DocImage back;
    CHECK(n > 100 && docImageFromData(data, n, 1000, 1000, &back) && back.width == 64 && back.height == 48);
    if (back.pixels) {
        uint32_t p = back.pixels[64 * 24 + 32];
        CHECK(abs((int)(p & 0xFF) - 0xC0) < 12 && abs((int)((p >> 8) & 0xFF) - 0x60) < 12 &&
              abs((int)((p >> 16) & 0xFF) - 0x20) < 12);
    }
    docImageFree(&back);
    DocImage none = {0};
    CHECK(!docImageSaveJpeg(&none, path, 82) && !docImageSaveJpeg(&image, path, 0));
    docImageFree(&image);
}

void testDoc(void)
{
    testSaveJpeg();
    testTallStrips();
    testCovers();
    testNames();
    testEpub();
    testOldEpub();
    testPictureColors();
    testFixed();
    testComicArchives();
    testRenderPart();
    testRemote();
    testOthers();
}
