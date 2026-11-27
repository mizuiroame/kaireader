// The library's covers: a few in memory (the ones on screen and near), and
// every one made kept on the SD (covers/ next to the NRO), so a book is
// opened only once for its cover. The reading thread makes them (reading.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "reading.h"

#define COVERS_MAX 48
#define COVERS_FOLDER_MAX 512

typedef struct {
    char path[SHELF_PATH_MAX];
    int64_t size;      // Of the book's file: another file under the name gets another cover.
    uint32_t *pixels;  // RGBA; NULL while it's being made, or if the book has none.
    int width, height;
    int made_w, made_h; // The size it was asked for (made to fit in): asked larger, it's made again.
    bool asked;        // The reading thread is making it.
    bool failed;       // The book has no cover we can show.
    uint64_t used;     // When last drawn (for which one to drop).
} Cover;

typedef struct {
    Cover items[COVERS_MAX];
    uint64_t tick;
    char folder[COVERS_FOLDER_MAX];  // Empty: none kept on the SD.
} Covers;

void coversInit(Covers *c, const char *folder);
void coversFree(Covers *c);

// The cover of a book of size bytes, made to w x h pixels at most: from
// memory, or the SD; if neither has it, the reading thread is asked for it
// and NULL comes back until it's there. NULL also for books without one
// (failed is then set, when given). One made for a smaller size than asked
// (a news picture first asked for small) is made again, the old one shown
// meanwhile.
const Cover *coversGet(Covers *c, const char *path, int64_t size, int w, int h, bool *failed);
// A cover the reading thread made: kept, and written to the SD. Takes the image.
void coversPut(Covers *c, ReadingCover *made, int64_t size);

// The file a book's cover is kept in, in folder: a hash of its path and size.
void coversFileName(const char *folder, const char *path, int64_t size, char *out, size_t out_size);
