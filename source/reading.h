// The reading thread: it owns the book open (doc.h), lays it out and renders
// the pages the screen asks for, so a slow page never stops the drawing.
// When it has nothing to read it makes the library's covers.
//
// Each open and each layout is a generation: the page numbers and pages
// that come back are of the generation they say, and the app drops the ones
// of a generation gone.
#pragma once

#include <stdbool.h>

#include "doc.h"
#include "shelf.h"

#define READING_WANT_MAX 4
#define READING_CHAPTERS_MAX 1024

typedef struct {
    float box_w, box_h;      // The room for a reflowable page, in points (one point is a layout unit).
    float fixed_w, fixed_h;  // The room a fixed page (PDF, comics) is fitted into.
    float em;            // Text size of a reflowable book, in points.
    float pixel_scale;   // Pixels per point.
    DocColors colors;
} ReadingLayout;

typedef enum {
    READING_CLOSED,
    READING_OPENING,  // Opening or laying out: the generation hasn't come yet.
    READING_OPEN,
    READING_FAILED,
} ReadingState;

typedef struct {
    ReadingState state;
    unsigned generation;  // Of what's below.
    char path[SHELF_PATH_MAX];
    char error[DOC_ERROR_MAX];
    char title[DOC_TEXT_MAX];
    char author[DOC_TEXT_MAX];
    bool reflowable;
    int pages;
    int start_page;     // The page of the place asked for when it opened or was laid out.
    int chapters;       // Entries in the table of contents (readingChapters).
    int current_page;   // The last page readingSetCurrent told, and where it is:
    DocPlace current_place;
    int current_percent;
} ReadingStatus;

typedef struct {
    unsigned generation;
    int page;
    DocImage image;  // The caller frees it (docImageFree) after it's taken.
} ReadingPage;

typedef struct {
    char path[SHELF_PATH_MAX];
    bool ok;
    DocImage image;  // Fits in the size asked; the caller frees it.
    char title[DOC_TEXT_MAX];
    char author[DOC_TEXT_MAX];
} ReadingCover;

bool readingStart(void);
void readingStop(void);  // Closes the book; pages and covers not taken are freed.

// Opens a book at a place; returns the generation it will have.
unsigned readingOpen(const char *path, const ReadingLayout *layout, DocPlace place);
// Lays the open book out again (a new text size or colors) and finds the place in it.
unsigned readingRelayout(const ReadingLayout *layout, DocPlace place);
void readingClose(void);

// The pages wanted now, most wanted first (the screen's page, then the next...);
// only ones of the current generation are rendered. Replaces the ones before.
void readingWant(unsigned generation, const int *pages, int count);
// The page shown now: the thread works out its place and how far in it is.
void readingSetCurrent(unsigned generation, int page);
// A page rendered; false if none is waiting.
bool readingTakePage(ReadingPage *out);

void readingGetStatus(ReadingStatus *out);
// The table of contents of the current generation, up to max entries; how many.
int readingChapters(DocChapter *out, int max);

// Asks for a book's cover, w x h pixels at most (the newest asks come first;
// a few are kept). A cover comes back failed if the book can't be opened.
void readingWantCover(const char *path, int w, int h);
bool readingTakeCover(ReadingCover *out);
// Drops the covers asked and not yet made (the library closed).
void readingForgetCovers(void);
