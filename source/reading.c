#include "reading.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_QUEUE 6
#define COVER_ASKS 12
#define COVER_QUEUE 4
#define THREAD_STACK (2u << 20)  // MuPDF's HTML layout goes deep.
// A book without a cover picture: its first page laid out like a paperback's.
#define COVER_PAGE_W 400.0f
#define COVER_PAGE_H 600.0f
#define COVER_EM 14.0f

typedef enum {
    CMD_NONE,
    CMD_OPEN,
    CMD_RELAYOUT,
    CMD_CLOSE,
} Command;

typedef struct {
    char path[SHELF_PATH_MAX];
    int w, h;
} CoverAsk;

static pthread_t g_thread;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_wake = PTHREAD_COND_INITIALIZER;
static bool g_started, g_quit;

// Asked by the app (under g_lock).
static Command g_cmd;
static char g_cmd_path[SHELF_PATH_MAX];
static ReadingLayout g_cmd_layout;
static DocPlace g_cmd_place;
static unsigned g_next_generation;
static unsigned g_want_generation;
static int g_want[READING_WANT_MAX];
static int g_want_count;
static unsigned g_current_generation;
static int g_current_page = -1;
static bool g_current_new;
static CoverAsk g_cover_asks[COVER_ASKS];  // Newest last.
static int g_cover_ask_count;

// Given back (under g_lock).
static ReadingStatus g_status;
static DocChapter *g_chapters;
static ReadingPage g_pages[PAGE_QUEUE];
static int g_page_count;
static ReadingCover *g_covers[COVER_QUEUE];
static int g_cover_count;

// The thread's own.
static Doc *g_doc;
static ReadingLayout g_layout;

static float fitScale(Doc *d, int page, const ReadingLayout *l)
{
    if (docReflowable(d))
        return l->pixel_scale;
    float w, h;
    if (!docPageSize(d, page, &w, &h))
        return l->pixel_scale;
    float s = l->fixed_w / w < l->fixed_h / h ? l->fixed_w / w : l->fixed_h / h;
    return s * l->pixel_scale;
}

static int percentOf(int page, int pages)
{
    return pages > 0 ? (page + 1) * 100 / pages : 0;
}

// Opens or lays out (without g_lock), then publishes the generation.
static void runCommand(Command cmd, const char *path, const ReadingLayout *layout, DocPlace place,
                       unsigned generation)
{
    char error[DOC_ERROR_MAX] = "";
    if (cmd == CMD_OPEN || cmd == CMD_CLOSE) {
        docClose(g_doc);
        g_doc = NULL;
    }
    if (cmd == CMD_CLOSE) {
        pthread_mutex_lock(&g_lock);
        memset(&g_status, 0, sizeof(g_status));
        g_status.generation = generation;  // Pages of the book closed were dropped before.
        free(g_chapters);
        g_chapters = NULL;
        pthread_mutex_unlock(&g_lock);
        return;
    }
    if (cmd == CMD_OPEN)
        g_doc = docOpen(path, error, sizeof(error));
    if (g_doc) {
        g_layout = *layout;
        if (!docLayout(g_doc, layout->box_w, layout->box_h, layout->em) && !error[0])
            snprintf(error, sizeof(error), "The book couldn't be laid out.");
        if (!error[0] && docPageCount(g_doc) <= 0)
            snprintf(error, sizeof(error), "The book has no pages.");
    }
    DocChapter *chapters = NULL;
    int chapter_count = 0;
    ReadingStatus st;
    memset(&st, 0, sizeof(st));
    st.generation = generation;
    snprintf(st.path, sizeof(st.path), "%s", path);
    if (!g_doc || error[0]) {
        st.state = READING_FAILED;
        snprintf(st.error, sizeof(st.error), "%s", error[0] ? error : "The book couldn't be opened.");
    } else {
        st.state = READING_OPEN;
        st.reflowable = docReflowable(g_doc);
        st.pages = docPageCount(g_doc);
        st.start_page = docPageOfPlace(g_doc, place);
        st.current_page = st.start_page;
        st.current_place = docPlaceOf(g_doc, st.start_page);
        st.current_percent = percentOf(st.start_page, st.pages);
        docTitle(g_doc, st.title, sizeof(st.title));
        docAuthor(g_doc, st.author, sizeof(st.author));
        chapters = malloc(READING_CHAPTERS_MAX * sizeof(*chapters));
        if (chapters)
            chapter_count = docChapters(g_doc, chapters, READING_CHAPTERS_MAX);
        st.chapters = chapter_count;
    }
    pthread_mutex_lock(&g_lock);
    if (generation == g_next_generation) {  // Not asked for something else meanwhile.
        g_status = st;
        free(g_chapters);
        g_chapters = chapters;
        chapters = NULL;
    }
    pthread_mutex_unlock(&g_lock);
    free(chapters);
}

// True if the page is waiting in the queue already.
static bool queued(unsigned generation, int page)
{
    for (int i = 0; i < g_page_count; i++)
        if (g_pages[i].generation == generation && g_pages[i].page == page)
            return true;
    return false;
}

// Drops pages of generations gone from the queue (under g_lock).
static void dropOldPages(unsigned generation)
{
    int kept = 0;
    for (int i = 0; i < g_page_count; i++) {
        if (g_pages[i].generation == generation)
            g_pages[kept++] = g_pages[i];
        else
            docImageFree(&g_pages[i].image);
    }
    g_page_count = kept;
}

static void renderPage(unsigned generation, int page)
{
    ReadingPage out = {generation, page, {0}};
    if (!docRender(g_doc, page, fitScale(g_doc, page, &g_layout), &g_layout.colors, &out.image))
        out.image = (DocImage){0};
    pthread_mutex_lock(&g_lock);
    if (generation == g_status.generation && g_page_count < PAGE_QUEUE) {
        g_pages[g_page_count++] = out;
        out.image.pixels = NULL;
    }
    pthread_mutex_unlock(&g_lock);
    docImageFree(&out.image);
}

static void makeCover(const CoverAsk *ask)
{
    ReadingCover *c = calloc(1, sizeof(*c));
    if (!c)
        return;
    snprintf(c->path, sizeof(c->path), "%s", ask->path);
    char error[DOC_ERROR_MAX];
    Doc *d = docOpen(ask->path, error, sizeof(error));
    if (d) {
        docTitle(d, c->title, sizeof(c->title));
        docAuthor(d, c->author, sizeof(c->author));
        c->ok = docEpubCover(ask->path, ask->w, ask->h, &c->image);
        float w, h;
        if (!c->ok)  // No cover picture: the first page, laid out like a paperback's.
            docLayout(d, COVER_PAGE_W, COVER_PAGE_H, COVER_EM);
        if (!c->ok && docPageCount(d) > 0 && docPageSize(d, 0, &w, &h)) {
            float s = ask->w / w < ask->h / h ? ask->w / w : ask->h / h;
            c->ok = docRender(d, 0, s, NULL, &c->image);
        }
        docClose(d);
    }
    pthread_mutex_lock(&g_lock);
    if (g_cover_count < COVER_QUEUE) {
        g_covers[g_cover_count++] = c;
        c = NULL;
    }
    pthread_mutex_unlock(&g_lock);
    if (c) {
        docImageFree(&c->image);
        free(c);
    }
}

static void *worker(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_lock);
    while (!g_quit) {
        if (g_cmd != CMD_NONE) {
            Command cmd = g_cmd;
            char path[SHELF_PATH_MAX];
            snprintf(path, sizeof(path), "%s", cmd == CMD_RELAYOUT ? g_status.path : g_cmd_path);
            ReadingLayout layout = g_cmd_layout;
            DocPlace place = g_cmd_place;
            unsigned generation = g_next_generation;
            g_cmd = CMD_NONE;
            g_status.state = cmd == CMD_CLOSE ? READING_CLOSED : READING_OPENING;
            dropOldPages(generation);
            pthread_mutex_unlock(&g_lock);
            runCommand(cmd, path, &layout, place, generation);
            pthread_mutex_lock(&g_lock);
            continue;
        }
        unsigned generation = g_status.generation;
        bool open = g_doc && g_status.state == READING_OPEN;
        if (open && g_current_new && g_current_generation == generation) {
            g_current_new = false;
            int page = g_current_page;
            pthread_mutex_unlock(&g_lock);
            DocPlace place = docPlaceOf(g_doc, page);
            pthread_mutex_lock(&g_lock);
            if (generation == g_status.generation) {
                g_status.current_page = page;
                g_status.current_place = place;
                g_status.current_percent = percentOf(page, g_status.pages);
            }
            continue;
        }
        int page = -1;
        if (open && g_want_generation == generation && g_page_count < PAGE_QUEUE) {
            for (int i = 0; i < g_want_count && page < 0; i++)
                if (g_want[i] >= 0 && g_want[i] < g_status.pages && !queued(generation, g_want[i]))
                    page = g_want[i];
        }
        if (page >= 0) {
            // Taken off the list: asked again next frame if it's still wanted (and not yet taken).
            for (int i = 0; i < g_want_count; i++)
                if (g_want[i] == page)
                    g_want[i] = -1;
            pthread_mutex_unlock(&g_lock);
            renderPage(generation, page);
            pthread_mutex_lock(&g_lock);
            continue;
        }
        if (g_cover_ask_count > 0 && g_cover_count < COVER_QUEUE) {
            CoverAsk ask = g_cover_asks[--g_cover_ask_count];
            pthread_mutex_unlock(&g_lock);
            makeCover(&ask);
            pthread_mutex_lock(&g_lock);
            continue;
        }
        pthread_cond_wait(&g_wake, &g_lock);
    }
    pthread_mutex_unlock(&g_lock);
    docClose(g_doc);
    g_doc = NULL;
    return NULL;
}

bool readingStart(void)
{
    if (g_started)
        return true;
    g_quit = false;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, THREAD_STACK);
    g_started = pthread_create(&g_thread, &attr, worker, NULL) == 0;
    pthread_attr_destroy(&attr);
    return g_started;
}

void readingStop(void)
{
    if (!g_started)
        return;
    pthread_mutex_lock(&g_lock);
    g_quit = true;
    pthread_cond_signal(&g_wake);
    pthread_mutex_unlock(&g_lock);
    pthread_join(g_thread, NULL);
    g_started = false;
    for (int i = 0; i < g_page_count; i++)
        docImageFree(&g_pages[i].image);
    g_page_count = 0;
    for (int i = 0; i < g_cover_count; i++) {
        docImageFree(&g_covers[i]->image);
        free(g_covers[i]);
    }
    g_cover_count = 0;
    g_cover_ask_count = 0;
    free(g_chapters);
    g_chapters = NULL;
    memset(&g_status, 0, sizeof(g_status));
    g_cmd = CMD_NONE;
}

static unsigned ask(Command cmd, const char *path, const ReadingLayout *layout, DocPlace place)
{
    pthread_mutex_lock(&g_lock);
    unsigned generation = ++g_next_generation;
    // An open waiting, then a new layout: it stays an open (with the new layout).
    if (!(cmd == CMD_RELAYOUT && g_cmd == CMD_OPEN))
        g_cmd = cmd;
    if (path)
        snprintf(g_cmd_path, sizeof(g_cmd_path), "%s", path);
    if (layout)
        g_cmd_layout = *layout;
    g_cmd_place = place;
    g_want_count = 0;
    g_status.state = cmd == CMD_CLOSE ? READING_CLOSED : READING_OPENING;
    pthread_cond_signal(&g_wake);
    pthread_mutex_unlock(&g_lock);
    return generation;
}

unsigned readingOpen(const char *path, const ReadingLayout *layout, DocPlace place)
{
    return ask(CMD_OPEN, path, layout, place);
}

unsigned readingRelayout(const ReadingLayout *layout, DocPlace place)
{
    return ask(CMD_RELAYOUT, NULL, layout, place);
}

void readingClose(void)
{
    ask(CMD_CLOSE, NULL, NULL, (DocPlace){0, 0});
}

void readingWant(unsigned generation, const int *pages, int count)
{
    pthread_mutex_lock(&g_lock);
    g_want_generation = generation;
    g_want_count = count < READING_WANT_MAX ? count : READING_WANT_MAX;
    memcpy(g_want, pages, (size_t)g_want_count * sizeof(*g_want));
    if (g_want_count)
        pthread_cond_signal(&g_wake);
    pthread_mutex_unlock(&g_lock);
}

void readingSetCurrent(unsigned generation, int page)
{
    pthread_mutex_lock(&g_lock);
    if (g_current_generation != generation || g_current_page != page) {
        g_current_generation = generation;
        g_current_page = page;
        g_current_new = true;
        pthread_cond_signal(&g_wake);
    }
    pthread_mutex_unlock(&g_lock);
}

bool readingTakePage(ReadingPage *out)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_page_count > 0;
    if (got) {
        *out = g_pages[0];
        memmove(g_pages, g_pages + 1, (size_t)(--g_page_count) * sizeof(*g_pages));
        pthread_cond_signal(&g_wake);  // Room for another.
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

void readingGetStatus(ReadingStatus *out)
{
    pthread_mutex_lock(&g_lock);
    *out = g_status;
    if (g_cmd != CMD_NONE || g_status.generation != g_next_generation)
        out->state = g_cmd == CMD_CLOSE ? READING_CLOSED : READING_OPENING;
    pthread_mutex_unlock(&g_lock);
}

int readingChapters(DocChapter *out, int max)
{
    pthread_mutex_lock(&g_lock);
    int n = g_chapters ? g_status.chapters : 0;
    n = n < max ? n : max;
    if (n > 0)
        memcpy(out, g_chapters, (size_t)n * sizeof(*out));
    pthread_mutex_unlock(&g_lock);
    return n;
}

void readingWantCover(const char *path, int w, int h)
{
    if (strlen(path) >= SHELF_PATH_MAX || w <= 0 || h <= 0)
        return;
    pthread_mutex_lock(&g_lock);
    // Asked again: it moves to the front.
    for (int i = 0; i < g_cover_ask_count; i++) {
        if (!strcmp(g_cover_asks[i].path, path)) {
            memmove(&g_cover_asks[i], &g_cover_asks[i + 1], (size_t)(g_cover_ask_count - i - 1) * sizeof(CoverAsk));
            g_cover_ask_count--;
            break;
        }
    }
    if (g_cover_ask_count == COVER_ASKS) {  // Full: the oldest ask goes.
        memmove(&g_cover_asks[0], &g_cover_asks[1], (COVER_ASKS - 1) * sizeof(CoverAsk));
        g_cover_ask_count--;
    }
    CoverAsk *a = &g_cover_asks[g_cover_ask_count++];
    snprintf(a->path, sizeof(a->path), "%s", path);
    a->w = w;
    a->h = h;
    pthread_cond_signal(&g_wake);
    pthread_mutex_unlock(&g_lock);
}

bool readingTakeCover(ReadingCover *out)
{
    pthread_mutex_lock(&g_lock);
    bool got = g_cover_count > 0;
    if (got) {
        *out = *g_covers[0];
        free(g_covers[0]);
        memmove(g_covers, g_covers + 1, (size_t)(--g_cover_count) * sizeof(*g_covers));
        pthread_cond_signal(&g_wake);
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

void readingForgetCovers(void)
{
    pthread_mutex_lock(&g_lock);
    g_cover_ask_count = 0;
    pthread_mutex_unlock(&g_lock);
}
