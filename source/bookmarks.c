#include "bookmarks.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAME_SPOT 1e-6  // Fractions this close are the same place (they come from the same page start).

void bookmarksFree(Bookmarks *b)
{
    free(b->items);
    b->items = NULL;
    b->count = b->cap = 0;
}

static bool grow(Bookmarks *b)
{
    if (b->count < b->cap)
        return true;
    if (b->cap >= BOOKMARKS_MAX)
        return false;
    int cap = b->cap ? b->cap * 2 : 16;
    if (cap > BOOKMARKS_MAX)
        cap = BOOKMARKS_MAX;
    Bookmark *items = realloc(b->items, (size_t)cap * sizeof(*items));
    if (!items)
        return false;
    b->items = items;
    b->cap = cap;
    return true;
}

static bool pathOk(const char *path)
{
    size_t n = path ? strlen(path) : 0;
    return n > 0 && n < SHELF_PATH_MAX && !strpbrk(path, "\t\r\n");
}

// One line: path TAB chapter TAB fraction TAB made. False if it isn't one.
static bool readLine(char *line, Bookmark *out)
{
    char *fields[4];
    int n = 0;
    for (char *p = line; n < 4; n++) {
        fields[n] = p;
        p = strchr(p, '\t');
        if (!p) {
            n++;
            break;
        }
        *p++ = '\0';
    }
    if (n != 4 || !pathOk(fields[0]))
        return false;
    char *end;
    long chapter = strtol(fields[1], &end, 10);
    if (end == fields[1] || *end || chapter < 0 || chapter > 100000)
        return false;
    double fraction = strtod(fields[2], &end);
    if (end == fields[2] || *end || !(fraction >= 0 && fraction <= 1))
        return false;
    long long made = strtoll(fields[3], &end, 10);
    if (end == fields[3] || *end || made < 0)
        return false;
    snprintf(out->path, sizeof(out->path), "%s", fields[0]);
    out->place = (DocPlace){(int)chapter, fraction};
    out->made = made;
    return true;
}

bool bookmarksLoad(Bookmarks *b, const char *file)
{
    b->count = 0;
    FILE *f = fopen(file, "r");
    if (!f)
        return errno == ENOENT;
    char line[SHELF_PATH_MAX + 96];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        Bookmark m;
        if (line[0] && line[0] != '#' && readLine(line, &m) && bookmarksFind(b, m.path, m.place) < 0 && grow(b))
            b->items[b->count++] = m;
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool bookmarksSave(const Bookmarks *b, const char *file)
{
    char tmp[SHELF_PATH_MAX + 8];
    if (snprintf(tmp, sizeof(tmp), "%s.new", file) >= (int)sizeof(tmp))
        return false;
    FILE *f = fopen(tmp, "w");
    if (!f)
        return false;
    fputs("# KaiReader bookmarks: path, chapter, fraction, when made\n", f);
    for (int i = 0; i < b->count; i++) {
        const Bookmark *m = &b->items[i];
        fprintf(f, "%s\t%d\t%.9f\t%lld\n", m->path, m->place.chapter, m->place.fraction, (long long)m->made);
    }
    bool ok = !ferror(f);
    if (fclose(f) != 0 || !ok) {
        remove(tmp);
        return false;
    }
    remove(file);  // The console's card can't rename over a file.
    return rename(tmp, file) == 0;
}

static int compareMarks(const void *a, const void *b, const Bookmarks *list)
{
    const Bookmark *x = &list->items[*(const int *)a], *y = &list->items[*(const int *)b];
    if (x->place.chapter != y->place.chapter)
        return x->place.chapter < y->place.chapter ? -1 : 1;
    return x->place.fraction < y->place.fraction ? -1 : x->place.fraction > y->place.fraction;
}

int bookmarksOf(const Bookmarks *b, const char *path, int *out, int max)
{
    int n = 0;
    for (int i = 0; i < b->count && n < max; i++)
        if (!strcmp(b->items[i].path, path))
            out[n++] = i;
    for (int i = 1; i < n; i++)  // Few: insertion sort, in reading order.
        for (int j = i; j > 0 && compareMarks(&out[j - 1], &out[j], b) > 0; j--) {
            int t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
    return n;
}

int bookmarksFind(const Bookmarks *b, const char *path, DocPlace place)
{
    for (int i = 0; i < b->count; i++) {
        const Bookmark *m = &b->items[i];
        double d = m->place.fraction - place.fraction;
        if (m->place.chapter == place.chapter && d < SAME_SPOT && d > -SAME_SPOT && !strcmp(m->path, path))
            return i;
    }
    return -1;
}

bool bookmarksAdd(Bookmarks *b, const char *path, DocPlace place, int64_t made)
{
    if (!pathOk(path) || place.chapter < 0 || !(place.fraction >= 0 && place.fraction <= 1) ||
        bookmarksFind(b, path, place) >= 0)
        return false;
    int mine[BOOKMARKS_PER_BOOK];
    if (bookmarksOf(b, path, mine, BOOKMARKS_PER_BOOK) >= BOOKMARKS_PER_BOOK || !grow(b))
        return false;
    Bookmark *m = &b->items[b->count++];
    snprintf(m->path, sizeof(m->path), "%s", path);
    m->place = place;
    m->made = made;
    return true;
}

void bookmarksRemove(Bookmarks *b, int index)
{
    if (index < 0 || index >= b->count)
        return;
    memmove(&b->items[index], &b->items[index + 1], (size_t)(b->count - index - 1) * sizeof(b->items[0]));
    b->count--;
}
