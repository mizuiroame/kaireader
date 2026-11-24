// A book open with MuPDF: EPUB, FB2, MOBI, XHTML and TXT, which flow into
// pages of the size asked (reflowable), and PDF, XPS and comic books (CBZ, CBR, CB7, CBT),
// whose pages are fixed. A Doc belongs to one thread: KaiReader's reading
// thread (reading.h); the tests use it directly.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DOC_ERROR_MAX 160
#define DOC_TEXT_MAX 256
// Rendered pages are never bigger than this on a side: a page made huge on
// purpose can't take all the memory.
#define DOC_RENDER_MAX 4096

typedef struct Doc Doc;

struct fz_context;
struct fz_stream;
#define DOC_REMOTE_PREFIX "dav:/"  // A book of the network folder: opened through the remote opener.
// Opens a "dav:/..." path as a stream MuPDF reads (it throws if it can't). Set by the app (netstream.c).
typedef struct fz_stream *(*DocRemoteOpener)(struct fz_context *ctx, const char *path);
void docSetRemoteOpener(DocRemoteOpener open);

// One entry of the table of contents.
typedef struct {
    char title[DOC_TEXT_MAX];
    int page;   // Page it starts on (in the layout it was read in); -1 if it points nowhere.
    int level;  // 0 for a chapter, 1 for a section in it...
} DocChapter;

// A rendered page: RGBA (0xAABBGGRR, opaque), width x height pixels, no padding.
typedef struct {
    uint32_t *pixels;
    int width, height;
} DocImage;

// The colors of a reflowable page: text and paper (0xRRGGBB). Fixed pages keep their own.
typedef struct {
    uint32_t ink, paper;
} DocColors;

// Fonts for the scripts books often don't bring: Japanese, simplified and
// traditional Chinese, Korean (the console's own, on Switch 2). Given once,
// before any book opens; the data must stay while books are open. A size of
// 0 leaves one out (those letters then show as boxes).
typedef struct {
    const void *data;
    size_t size;
} DocFont;

typedef enum {
    DOC_FONT_JAPANESE,
    DOC_FONT_CHINESE_SIMPLIFIED,
    DOC_FONT_CHINESE_TRADITIONAL,
    DOC_FONT_KOREAN,
    DOC_FONT_COUNT,
} DocFontKind;

void docSetFallbackFonts(const DocFont fonts[DOC_FONT_COUNT]);

// Opens the file at path; NULL with a reason in error if it can't. The
// formats are the ones docKnownName accepts. A reflowable book's text is
// given colors->ink (NULL: as the book has it), over colors->paper when
// rendered; its pictures keep theirs. Other colors need it opened again.
Doc *docOpen(const char *path, const DocColors *colors, char *error, size_t error_size);
void docClose(Doc *d);

// True for the names of files KaiReader can open (by extension).
bool docKnownName(const char *name);

// True if the book flows into pages of any size (EPUB...); false if its pages are fixed (PDF, comics).
bool docReflowable(const Doc *d);
// Flows a reflowable book into pages of w x h points, with text of em points (fixed ones don't change).
// False if it failed (the layout before stays).
bool docLayout(Doc *d, float w, float h, float em);
// The same, keeping the reader's place exactly: *page (in the layout before)
// becomes the page that now holds the text that started it. A DocPlace is
// only near (chapter and fraction of its pages); this is MuPDF's own mark of
// the spot, good while the book stays open.
bool docLayoutKeeping(Doc *d, float w, float h, float em, int *page);
int docPageCount(Doc *d);
// A page's text, UTF-8, lines joined by spaces, up to size - 1 bytes (whole
// characters). Empty for a page without text (a picture, a scanned PDF).
// False if the page can't be read.
bool docPageText(Doc *d, int page, char *out, size_t size);
// A page's size in points; false if there's no such page.
bool docPageSize(Doc *d, int page, float *w, float *h);
// Renders a page at scale pixels per point: a reflowable one over the paper
// it was opened with; a fixed one (PDF, comics) as it is. Frees with
// docImageFree. False if it failed.
bool docRender(Doc *d, int page, float scale, DocImage *out);
void docImageFree(DocImage *image);
// The scale docRender really renders a page at when asked scale: smaller when the
// page would be over DOC_RENDER_MAX pixels a side.
float docRenderScale(Doc *d, int page, float scale);
// Part of a fixed page at scale, not capped: its pixels x, y (from the whole
// page's top left), w x h (each DOC_RENDER_MAX at most). False if it fails.
bool docRenderPart(Doc *d, int page, float scale, int x, int y, int w, int h, DocImage *out);

// What an EPUB's package says beyond the title: its cover picture (EPUB 3's
// cover-image, or EPUB 2's <meta name="cover">) fitted into max_w x max_h,
// its first subject (the genre), and its author as sorted ("Example, Ana",
// from file-as). Empty or NULL pixels for what it doesn't say.
typedef struct {
    DocImage cover;
    char genre[DOC_TEXT_MAX];
    char author_sort[DOC_TEXT_MAX];
} DocEpubInfo;

// False for files that aren't EPUBs or whose package can't be read.
bool docEpubInfo(const char *path, int max_w, int max_h, DocEpubInfo *out);
// Only the cover: false if there's none.
bool docEpubCover(const char *path, int max_w, int max_h, DocImage *out);

// The genre a PDF's Subject gives (empty if none). False if there's none.
bool docSubject(Doc *d, char *out, size_t size);

// The title or author the book gives (empty if none). False if there's none.
bool docTitle(Doc *d, char *out, size_t size);
bool docAuthor(Doc *d, char *out, size_t size);

// The table of contents, flattened in reading order: up to max chapters into
// out; how many. 0 if the book has none.
int docChapters(Doc *d, DocChapter *out, int max);

// A place in the book that stays the same after another layout (a new text
// size) and after KaiReader closes: the chapter (a file inside an EPUB; 0 for
// a PDF) and how far into it, 0 to 1.
typedef struct {
    int chapter;
    double fraction;
} DocPlace;

DocPlace docPlaceOf(Doc *d, int page);
// The page a place is on now (0 for one the book doesn't have).
int docPageOfPlace(Doc *d, DocPlace place);
