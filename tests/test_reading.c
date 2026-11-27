// reading.c: the reading thread with the made-up books.
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "reading.h"

#define FIX "build/fixtures/"

static void nap(void)
{
    struct timespec ts = {0, 5 * 1000 * 1000};
    nanosleep(&ts, NULL);
}

// Waits (up to 10 s) for the generation to be open or failed.
static bool waitFor(unsigned generation, ReadingStatus *st)
{
    for (int i = 0; i < 2000; i++) {
        readingGetStatus(st);
        if (st->generation == generation && (st->state == READING_OPEN || st->state == READING_FAILED))
            return true;
        nap();
    }
    return false;
}

static bool takePage(ReadingPage *p)
{
    for (int i = 0; i < 2000; i++) {
        if (readingTakePage(p))
            return true;
        nap();
    }
    return false;
}

static bool takeCover(ReadingCover *c)
{
    for (int i = 0; i < 2000; i++) {
        if (readingTakeCover(c))
            return true;
        nap();
    }
    return false;
}

void testReading(void)
{
    CHECK(readingStart());
    ReadingLayout layout = {400, 600, 400, 600, 14, 1.0f, {0x000000, 0xFFFFFF}, 1.0f, false};
    ReadingStatus st;
    unsigned g = readingOpen(FIX "quiet.epub", &layout, (DocPlace){2, 0.0});
    CHECK(waitFor(g, &st) && st.state == READING_OPEN);
    CHECK(st.reflowable && st.pages > 5 && st.chapters == 3 && !strcmp(st.title, "The Quiet Lighthouse"));
    DocChapter ch[8];
    CHECK(readingChapters(ch, 8) == 3);
    CHECK(st.start_page == ch[2].page);  // The place asked: chapter 3's start.

    int want[2] = {st.start_page, st.start_page + 1};
    readingWant(g, want, 2);
    ReadingPage p;
    CHECK(takePage(&p) && p.generation == g && p.page == st.start_page && p.image.width == 400);
    docImageFree(&p.image);
    CHECK(takePage(&p) && p.page == st.start_page + 1);
    docImageFree(&p.image);

    readingSetCurrent(g, st.start_page + 1);
    bool told = false;
    for (int i = 0; i < 2000 && !told; i++) {
        readingGetStatus(&st);
        told = st.current_page == want[1];
        nap();
    }
    CHECK(told && st.current_place.chapter == 2 && st.current_percent > 50);

    // Bookmarks: each one's page and first words, once the thread has found them.
    DocPlace marks[2] = {{2, 0.0}, st.current_place};
    readingSetMarks(marks, 2);
    ReadingMark found[4];
    int n = -1;
    for (int i = 0; i < 2000 && n < 0; i++) {
        n = readingMarks(g, found, 4);
        if (n < 0)
            nap();
    }
    CHECK(n == 2);
    if (n == 2) {
        CHECK(found[0].page == ch[2].page && found[1].page == want[1]);
        CHECK(found[0].text[0] && found[1].text[0] && strcmp(found[0].text, found[1].text));
    }
    CHECK(readingMarks(g + 99, found, 4) == -1);  // Not for another generation.

    // A bigger text: a new generation, the same chapter.
    layout.em = 28;
    unsigned g2 = readingRelayout(&layout, st.current_place);
    CHECK(g2 != g);
    readingWant(g, want, 1);  // Of the old generation: nothing comes.
    CHECK(waitFor(g2, &st) && st.state == READING_OPEN && st.pages > 5);
    CHECK(readingChapters(ch, 8) == 3 && st.start_page >= ch[2].page);
    CHECK(!readingTakePage(&p));

    // Fixed pages fit the room: 612 x 792 into 400 x 600 (by its width) at 2 pixels per point.
    layout.pixel_scale = 2;
    g = readingOpen(FIX "plain.pdf", &layout, (DocPlace){0, 0.5});
    CHECK(waitFor(g, &st) && st.state == READING_OPEN && !st.reflowable && st.pages == 3);
    want[0] = 1;
    readingWant(g, want, 1);
    CHECK(takePage(&p) && p.page == 1 && p.image.width == 800 && p.image.height >= 1034 && p.image.height <= 1036);
    docImageFree(&p.image);
    CHECK(!p.part && p.zoom == 1.0f);

    // Part of it at 8x (6400 pixels wide whole: too big for one image).
    ReadingPartAsk ask = {1, 8.0f, 2000, 3000, 640, 360};
    readingWantParts(g, &ask, 1);
    CHECK(takePage(&p) && p.part && p.page == 1 && p.zoom == 8.0f && p.x == 2000 && p.y == 3000);
    CHECK(p.image.width == 640 && p.image.height == 360 && p.full_w == 6400 && p.full_h >= 8270);
    docImageFree(&p.image);

    g = readingOpen(FIX "missing.epub", &layout, (DocPlace){0, 0});
    CHECK(waitFor(g, &st) && st.state == READING_FAILED && st.error[0]);

    // Covers: the newest ask first, title and author with it.
    readingWantCover(FIX "strip.cbz", 100, 150);
    readingWantCover(FIX "quiet.epub", 100, 150);
    ReadingCover c;
    CHECK(takeCover(&c) && c.ok && c.image.width <= 100 && c.image.height <= 150);
    CHECK(!strcmp(c.path, FIX "quiet.epub") || !strcmp(c.path, FIX "strip.cbz"));
    docImageFree(&c.image);
    CHECK(takeCover(&c) && c.ok);
    docImageFree(&c.image);
    readingWantCover(FIX "empty.pdf", 100, 150);
    CHECK(takeCover(&c) && !c.ok && !c.image.pixels);

    readingClose();
    for (int i = 0; i < 200; i++) {
        readingGetStatus(&st);
        if (st.state == READING_CLOSED)
            break;
        nap();
    }
    CHECK(st.state == READING_CLOSED);
    readingStop();
    readingStop();
}
