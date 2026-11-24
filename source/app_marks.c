// Bookmarks in the reader: marking the page shown, the open book's list (the
// Bookmarks tab beside Contents), and going to one. The places are kept in
// bookmarks.txt (bookmarks.h); the reading thread finds their pages and first
// words in the layout of the moment (readingSetMarks).
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "lang.h"

static void marksPath(const App *app, char *out, size_t size)
{
    snprintf(out, size, "%s/bookmarks.txt", app->folder);
}

void appMarksLoad(App *app)
{
    char path[APP_FOLDER_MAX + 32];
    marksPath(app, path, sizeof(path));
    bookmarksLoad(&app->marks, path);
}

static void saveMarks(App *app, uint64_t now_ns)
{
    char path[APP_FOLDER_MAX + 32];
    marksPath(app, path, sizeof(path));
    if (!bookmarksSave(&app->marks, path))
        appNotify(app, NOTICE_ERROR, T("The bookmarks couldn't be saved."), now_ns);
}

void appMarksForBook(App *app)
{
    app->book_mark_count = 0;
    app->mark_found_count = -1;
    if (app->book < 0)
        return;
    const char *path = app->shelf.items[app->book].path;
    app->book_mark_count = bookmarksOf(&app->marks, path, app->book_marks, READING_MARKS_MAX);
    DocPlace places[READING_MARKS_MAX];
    for (int i = 0; i < app->book_mark_count; i++)
        places[i] = app->marks.items[app->book_marks[i]].place;
    readingSetMarks(places, app->book_mark_count);
}

void appMarksUpdate(App *app)
{
    if (app->mark_generation != app->generation) {  // A new layout: their pages are found again.
        app->mark_generation = app->generation;
        app->mark_found_count = -1;
    }
    if (app->mark_found_count < 0)
        app->mark_found_count = readingMarks(app->generation, app->mark_found, READING_MARKS_MAX);
}

// The bookmark shown on page, or -1 (also while their pages aren't known yet).
static int markOnPage(const App *app, int page)
{
    for (int i = 0; i < app->mark_found_count && i < app->book_mark_count; i++)
        if (app->mark_found[i].page == page)
            return i;
    return -1;
}

bool appPageMarked(const App *app, int page)
{
    return markOnPage(app, page) >= 0;
}

bool appShownMarked(const App *app)
{
    return appPageMarked(app, app->page) || (appTwoPages(app) && appPageMarked(app, app->page + 1));
}

void appToggleMark(App *app, uint64_t now_ns)
{
    if (app->view != VIEW_READER || !app->ready || app->book < 0)
        return;
    if (app->mark_found_count < 0) {
        appNotify(app, NOTICE_INFO, T("One moment: the bookmarks are still being found."), now_ns);
        return;
    }
    if (appShownMarked(app)) {  // Off: every bookmark on the pages shown.
        int last = app->page + (appTwoPages(app) ? 1 : 0);
        // From the last index in the whole list down, so removing one doesn't move the others.
        for (int index = app->marks.count - 1; index >= 0; index--)
            for (int i = 0; i < app->mark_found_count && i < app->book_mark_count; i++)
                if (app->book_marks[i] == index && app->mark_found[i].page >= app->page &&
                    app->mark_found[i].page <= last) {
                    bookmarksRemove(&app->marks, index);
                    break;
                }
        appMarksForBook(app);
        saveMarks(app, now_ns);
        appNotify(app, NOTICE_INFO, T("Bookmark removed."), now_ns);
        return;
    }
    ReadingStatus st;
    readingGetStatus(&st);
    if (st.generation != app->generation || st.current_page != app->page) {
        appNotify(app, NOTICE_INFO, T("One moment: the page is still being read."), now_ns);
        return;
    }
    const char *path = app->shelf.items[app->book].path;
    if (!bookmarksAdd(&app->marks, path, st.current_place, (int64_t)time(NULL))) {
        appNotify(app, NOTICE_ERROR, T("No more bookmarks fit in this book."), now_ns);
        return;
    }
    saveMarks(app, now_ns);
    appMarksForBook(app);
    appNotify(app, NOTICE_INFO, T("Bookmark added. Press X, then R, to see them all."), now_ns);
}

void appMarksGo(App *app, int i)
{
    if (i < 0 || i >= app->mark_found_count || i >= app->book_mark_count)
        return;
    int page = app->mark_found[i].page;
    app->panel = PANEL_NONE;
    if (page >= 0 && page < app->pages) {
        app->page = appFirstShown(app, page);
        app->turn_from = -1;
    }
}

void appMarksRemoveAt(App *app, int i, uint64_t now_ns)
{
    if (i < 0 || i >= app->book_mark_count)
        return;
    bookmarksRemove(&app->marks, app->book_marks[i]);
    saveMarks(app, now_ns);
    appMarksForBook(app);
    appNotify(app, NOTICE_INFO, T("Bookmark removed."), now_ns);
}
