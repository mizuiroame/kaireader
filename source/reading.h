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

#define READING_WANT_MAX 6
#define READING_CHAPTERS_MAX 1024
#define READING_MARKS_MAX 64     // A book's bookmarks the thread finds pages for.
#define READING_MARK_TEXT 120    // The first words of a bookmark's page.

typedef struct {
    float box_w, box_h;      // The room for a reflowable page, in points (one point is a layout unit).
    float fixed_w, fixed_h;  // The room a fixed page (PDF, comics) is fitted into.
    float em;            // Text size of a reflowable book, in points.
    float pixel_scale;   // Pixels per point.
    DocColors colors;
    float zoom;          // Fixed pages rendered this much bigger than fitted (1, or 0, for fitted).
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
    float zoom;      // A fixed page: how much bigger than fitted it came (less than asked if it'd be too big).
    bool part;       // Only part of the page (readingWantParts): from x, y of the whole, full_w x full_h.
    int x, y, full_w, full_h;
} ReadingPage;

// Part of a fixed page wanted at a zoom: its pixels x, y, w x h at that zoom.
typedef struct {
    int page;
    float zoom;
    int x, y, w, h;
} ReadingPartAsk;
#define READING_PARTS_MAX 2

// A bookmark in the layout of the moment: its page, and the words that start it.
typedef struct {
    int page;
    char text[READING_MARK_TEXT];
} ReadingMark;

typedef struct {
    char path[SHELF_PATH_MAX];
    bool ok;
    DocImage image;  // Fits in the size asked; the caller frees it.
    char title[DOC_TEXT_MAX];
    char author[DOC_TEXT_MAX];
    char genre[DOC_TEXT_MAX];        // An EPUB's first subject, a PDF's Subject.
    char author_sort[DOC_TEXT_MAX];  // "Example, Ana", when the book gives it.
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
// Parts of the pages shown wanted sharp at a zoom their whole pages can't come at
// (after the pages wanted). Replaces the ones before.
void readingWantParts(unsigned generation, const ReadingPartAsk *asks, int count);
// The page shown now: the thread works out its place and how far in it is.
void readingSetCurrent(unsigned generation, int page);
// The open book's bookmarks (their places, in reading order), replacing the ones before.
void readingSetMarks(const DocPlace *places, int count);
// For generation: each bookmark's page and first words, in the order given; how many.
// -1 while they're still being found (a new layout, or new bookmarks).
int readingMarks(unsigned generation, ReadingMark *out, int max);
// A page rendered; false if none is waiting.
bool readingTakePage(ReadingPage *out);

void readingGetStatus(ReadingStatus *out);
// The table of contents of the current generation, up to max entries; how many.
int readingChapters(DocChapter *out, int max);

// Asks for a book's cover, w x h pixels at most (the newest asks come first;
// a few are kept). A cover comes back failed if the book can't be opened.
void readingWantCover(const char *path, int w, int h);
bool readingTakeCover(ReadingCover *out);
// True while covers are asked for or being made.
bool readingCoversPending(void);
// Drops the covers asked and not yet made (the library closed).
void readingForgetCovers(void);
