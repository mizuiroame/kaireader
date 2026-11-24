#include "shelf.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

#define LINE_MAX_BYTES (SHELF_PATH_MAX + 2 * SHELF_TEXT_MAX + 128)

void shelfFree(Shelf *s)
{
    free(s->items);
    memset(s, 0, sizeof(*s));
}

// Copies text into out, tabs and line breaks made spaces (they separate the file's fields).
static void copyField(char *out, size_t size, const char *text)
{
    size_t i = 0;
    for (; text && text[i] && i + 1 < size; i++) {
        char c = text[i];
        out[i] = (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
    }
    out[i] = 0;
    // Never end inside a UTF-8 sequence cut short.
    size_t start = i;
    while (start > 0 && i - start < 4 && ((unsigned char)out[start - 1] & 0xC0) == 0x80)
        start--;
    if (start > 0) {
        unsigned char lead = (unsigned char)out[start - 1];
        size_t want = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (i - (start - 1) < want)
            out[start - 1] = 0;
    }
}

static ShelfBook *grow(Shelf *s)
{
    if (s->count >= SHELF_MAX_BOOKS)
        return NULL;
    if (s->count == s->cap) {
        int cap = s->cap ? s->cap * 2 : 64;
        ShelfBook *items = realloc(s->items, (size_t)cap * sizeof(*items));
        if (!items)
            return NULL;
        s->items = items;
        s->cap = cap;
    }
    ShelfBook *b = &s->items[s->count++];
    memset(b, 0, sizeof(*b));
    b->percent = -1;
    return b;
}

ShelfBook *shelfFind(Shelf *s, const char *path)
{
    for (int i = 0; i < s->count; i++)
        if (!strcmp(s->items[i].path, path))
            return &s->items[i];
    return NULL;
}

ShelfBook *shelfAdd(Shelf *s, const char *path)
{
    if (!path[0] || strlen(path) >= SHELF_PATH_MAX)
        return NULL;
    ShelfBook *b = shelfFind(s, path);
    if (b)
        return b;
    b = grow(s);
    if (!b)
        return NULL;
    copyField(b->path, sizeof(b->path), path);
    b->present = true;
    return b;
}

// Splits line at tabs into up to max fields (the line is changed).
static int split(char *line, char **fields, int max)
{
    int n = 0;
    char *p = line;
    while (n < max) {
        fields[n++] = p;
        char *tab = strchr(p, '\t');
        if (!tab)
            break;
        *tab = 0;
        p = tab + 1;
    }
    return n;
}

// One line: path, size, chapter, fraction, percent, last read, title, author, genre,
// author's sort name (tab-separated; the last two since 0.2).
static void readLine(Shelf *s, char *line)
{
    line[strcspn(line, "\r\n")] = 0;
    if (!line[0] || line[0] == '#')
        return;
    char *f[11] = {0};
    if (split(line, f, 11) < 6 || shelfFind(s, f[0]))
        return;
    ShelfBook *b = shelfAdd(s, f[0]);
    if (!b)
        return;
    b->present = false;  // Until a scan finds it.
    b->size = strtoll(f[1], NULL, 10);
    b->place.chapter = atoi(f[2]);
    b->place.fraction = strtod(f[3], NULL);
    if (b->place.chapter < 0 || !(b->place.fraction >= 0 && b->place.fraction <= 1))
        b->place = (DocPlace){0, 0};
    b->percent = atoi(f[4]);
    if (b->percent < -1 || b->percent > 100)
        b->percent = -1;
    b->last_read = strtoll(f[5], NULL, 10);
    if (b->last_read < 0)
        b->last_read = 0;
    copyField(b->title, sizeof(b->title), f[6]);
    copyField(b->author, sizeof(b->author), f[7]);
    copyField(b->genre, sizeof(b->genre), f[8]);
    copyField(b->author_sort, sizeof(b->author_sort), f[9]);
    b->favorite = f[10] && f[10][0] == '1';
}

bool shelfLoad(Shelf *s, const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f)
        return errno == ENOENT;
    char line[LINE_MAX_BYTES];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == sizeof(line) - 1 && line[len - 1] != '\n') {  // Too long: not one of ours; skip it whole.
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') {
            }
            continue;
        }
        readLine(s, line);
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool shelfSave(const Shelf *s, const char *file)
{
    char tmp[SHELF_PATH_MAX + 8];
    if (snprintf(tmp, sizeof(tmp), "%s.new", file) >= (int)sizeof(tmp))
        return false;
    FILE *f = fopen(tmp, "w");
    if (!f)
        return false;
    fprintf(f, "# KaiReader: path, size, chapter, fraction, percent, last read, title, author, genre, "
               "author sorted, favorite\n");
    for (int i = 0; i < s->count; i++) {
        const ShelfBook *b = &s->items[i];
        if (!b->present && b->percent < 0 && !b->favorite)
            continue;
        fprintf(f, "%s\t%lld\t%d\t%.6f\t%d\t%lld\t%s\t%s\t%s\t%s\t%d\n", b->path, (long long)b->size,
                b->place.chapter, b->place.fraction, b->percent, (long long)b->last_read, b->title, b->author,
                b->genre, b->author_sort, b->favorite ? 1 : 0);
    }
    bool ok = !ferror(f);
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp);
        return false;
    }
    remove(file);  // newlib's rename doesn't replace a file on the SD.
    return rename(tmp, file) == 0;
}

static void scanFolder(Shelf *s, const char *folder, int depth)
{
    DIR *dir = opendir(folder);
    if (!dir)
        return;
    struct dirent *e;
    char path[SHELF_PATH_MAX];
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", folder, e->d_name) >= (int)sizeof(path))
            continue;
        struct stat st;
        if (stat(path, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < SHELF_SCAN_DEPTH)
                scanFolder(s, path, depth + 1);
            continue;
        }
        if (!S_ISREG(st.st_mode) || !docKnownName(e->d_name))
            continue;
        ShelfBook *b = shelfAdd(s, path);
        if (!b)
            break;
        if (b->size != (int64_t)st.st_size && b->percent >= 0 && b->size) {
            // Another file under the same name: what was kept is about the old one.
            b->place = (DocPlace){0, 0};
            b->percent = -1;
            b->last_read = 0;
            b->title[0] = b->author[0] = b->genre[0] = b->author_sort[0] = 0;
        }
        b->size = (int64_t)st.st_size;
        b->present = true;
    }
    closedir(dir);
}

int shelfScan(Shelf *s, const char *folder)
{
    for (int i = 0; i < s->count; i++)
        s->items[i].present = false;
    char root[SHELF_PATH_MAX];
    copyField(root, sizeof(root), folder);
    size_t len = strlen(root);
    while (len > 1 && root[len - 1] == '/' && root[len - 2] != ':')
        root[--len] = 0;
    if (len > 0 && root[len - 1] == '/')
        root[--len] = 0;
    scanFolder(s, root, 1);
    int present = 0;
    for (int i = 0; i < s->count; i++)
        present += s->items[i].present;
    return present;
}

static const Shelf *g_sorting;
static ShelfSort g_sort;

// The name a book's author is sorted by: its sort name, or the name as written.
static const char *authorKey(const ShelfBook *b)
{
    return b->author_sort[0] ? b->author_sort : b->author;
}

// Empty ones last; the rest by name. 0 if they're the same.
static int compareText(const char *a, const char *b)
{
    if (!a[0] != !b[0])
        return a[0] ? -1 : 1;
    return utilCompareNames(a, b);
}

static int compareBooks(const void *pa, const void *pb)
{
    const ShelfBook *a = &g_sorting->items[*(const int *)pa], *b = &g_sorting->items[*(const int *)pb];
    int c = 0;
    if (g_sort == SHELF_SORT_RECENT) {
        bool ra = a->last_read > 0, rb = b->last_read > 0;
        if (ra != rb)
            return ra ? -1 : 1;
        if (ra && a->last_read != b->last_read)
            return a->last_read > b->last_read ? -1 : 1;
    } else if (g_sort == SHELF_SORT_GENRE) {
        c = compareText(a->genre, b->genre);
        if (!c)
            c = compareText(authorKey(a), authorKey(b));
    } else if (g_sort == SHELF_SORT_AUTHOR) {
        c = compareText(authorKey(a), authorKey(b));
    }
    if (c)
        return c;
    char na[SHELF_TEXT_MAX], nb[SHELF_TEXT_MAX];
    shelfDisplayName(a, na, sizeof(na));
    shelfDisplayName(b, nb, sizeof(nb));
    c = utilCompareNames(na, nb);
    return c ? c : strcmp(a->path, b->path);
}

int shelfOrder(const Shelf *s, ShelfSort sort, int *order, int max)
{
    int n = 0;
    for (int i = 0; i < s->count && n < max; i++)
        if (s->items[i].present && (sort != SHELF_SORT_FAVORITES || s->items[i].favorite))
            order[n++] = i;
    g_sorting = s;
    g_sort = sort >= 0 && sort < SHELF_SORT_COUNT ? sort : SHELF_SORT_RECENT;
    qsort(order, (size_t)n, sizeof(*order), compareBooks);
    g_sorting = NULL;
    return n;
}

void shelfGroupOf(const ShelfBook *b, ShelfSort sort, char *out, size_t size)
{
    if (sort == SHELF_SORT_GENRE)
        copyField(out, size, b->genre);
    else if (sort == SHELF_SORT_AUTHOR)
        copyField(out, size, b->author[0] ? b->author : b->author_sort);
    else if (size)
        out[0] = 0;
}

void shelfDisplayName(const ShelfBook *b, char *out, size_t size)
{
    if (b->title[0]) {
        copyField(out, size, b->title);
        return;
    }
    const char *slash = strrchr(b->path, '/');
    const char *name = slash ? slash + 1 : b->path;
    const char *dot = strrchr(name, '.');
    size_t len = dot && dot != name ? (size_t)(dot - name) : strlen(name);
    char tmp[SHELF_PATH_MAX];
    if (len >= sizeof(tmp))
        len = sizeof(tmp) - 1;
    memcpy(tmp, name, len);
    tmp[len] = 0;
    for (size_t i = 0; i < len; i++)
        if (tmp[i] == '_')
            tmp[i] = ' ';
    copyField(out, size, tmp);
}

void shelfMarkRead(ShelfBook *b, DocPlace place, int percent, int64_t now)
{
    b->place = place;
    b->percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    b->last_read = now;
}

void shelfSetDetails(ShelfBook *b, const char *genre, const char *author_sort)
{
    if (genre && genre[0])
        copyField(b->genre, sizeof(b->genre), genre);
    if (author_sort && author_sort[0])
        copyField(b->author_sort, sizeof(b->author_sort), author_sort);
}

void shelfSetInfo(ShelfBook *b, const char *title, const char *author)
{
    copyField(b->title, sizeof(b->title), title ? title : "");
    copyField(b->author, sizeof(b->author), author ? author : "");
}
