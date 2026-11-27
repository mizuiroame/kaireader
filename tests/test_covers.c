// covers.c with the reading thread: a news picture is kept at the size it's asked for, and one kept
// smaller (asked first at a book cover's size, or by an older KaiReader) is made again larger.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "check.h"
#include "covers.h"
#include "reading.h"

// The drawing's: nothing to forget on the PC.
void gfxImageChanged(const uint32_t *pixels)
{
    (void)pixels;
}

// A made-up news picture, 1200 x 675, fitted into the size asked (as opdsFetchCover does it).
static bool fakePicture(const char *path, int w, int h, DocImage *out)
{
    (void)path;
    float s = 1200.0f / w > 675.0f / h ? (float)w / 1200.0f : (float)h / 675.0f;
    s = s > 1 ? 1 : s;
    out->width = (int)(1200 * s + 0.5f);
    out->height = (int)(675 * s + 0.5f);
    out->pixels = calloc((size_t)out->width * out->height, 4);
    return out->pixels != NULL;
}

// Asked for until it's there at least w wide (the reading thread makes it meanwhile).
static const Cover *waitCover(Covers *c, const char *path, int w, int h)
{
    for (int i = 0; i < 2000; i++) {
        ReadingCover made;
        while (readingTakeCover(&made))
            coversPut(c, &made, 2);
        const Cover *cv = coversGet(c, path, 2, w, h, NULL);
        if (cv && cv->width >= w)
            return cv;
        struct timespec ts = {0, 2000000};
        nanosleep(&ts, NULL);
    }
    return coversGet(c, path, 2, w, h, NULL);
}

void testCoverCache(void)
{
    const char *folder = TMP_ROOT "/covers";
    mkdir(TMP_ROOT, 0755);
    mkdir(folder, 0755);
    char clean[256];
    snprintf(clean, sizeof(clean), "rm -f %s/*.cover", folder);
    CHECK(system(clean) == 0);
    CHECK(readingStart());
    readingSetCoverFetcher(fakePicture);
    const char *path = "news:0123456789abcdef/0123456789abcdef#lead";

    // Asked first at a book cover's size (240 x 360): made 240 x 135.
    Covers c;
    coversInit(&c, folder);
    const Cover *cv = waitCover(&c, path, 240, 360);
    CHECK(cv && cv->width == 240 && cv->height == 135);
    // Then at the lead story's size: made again, 839 wide, and kept so on the card.
    cv = waitCover(&c, path, 839, 839);
    CHECK(cv && cv->width == 839 && cv->height == 472);
    coversFree(&c);

    Covers again;  // Read back from the card: at that size, not asked for again.
    coversInit(&again, folder);
    cv = coversGet(&again, path, 2, 839, 839, NULL);
    CHECK(cv && cv->width == 839 && cv->height == 472 && !cv->asked);
    cv = coversGet(&again, path, 2, 400, 400, NULL);  // Smaller asked: the larger one does.
    CHECK(cv && cv->width == 839 && !cv->asked);
    coversFree(&again);

    // A cover an older KaiReader kept ("KRC1", 240 x 135): made again at the size asked now.
    char file[512];
    coversFileName(folder, "news:old/0000000000000000", 2, file, sizeof(file));
    FILE *f = fopen(file, "wb");
    if (f) {
        uint32_t w = 240, h = 135;
        fwrite("KRC1", 1, 4, f);
        fwrite(&w, 4, 1, f);
        fwrite(&h, 4, 1, f);
        uint32_t *pixels = calloc((size_t)w * h, 4);
        if (pixels)
            fwrite(pixels, 4, (size_t)w * h, f);
        free(pixels);
        fclose(f);
    }
    Covers old;
    coversInit(&old, folder);
    cv = coversGet(&old, "news:old/0000000000000000", 2, 429, 429, NULL);
    CHECK(cv && cv->width == 240);  // Shown meanwhile,
    cv = waitCover(&old, "news:old/0000000000000000", 429, 429);
    CHECK(cv && cv->width == 429 && cv->height == 241);  // then made again.
    coversFree(&old);
    readingSetCoverFetcher(NULL);
    readingStop();
}
