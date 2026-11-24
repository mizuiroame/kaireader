// shelf.c: finding books in folders, and keeping where each was left.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "check.h"
#include "shelf.h"

#define ROOT TMP_ROOT "/shelf"

static void writeFile(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void makeTree(void)
{
    if (system("rm -rf " ROOT) != 0) {
    }
    mkdir(TMP_ROOT, 0755);
    mkdir(ROOT, 0755);
    mkdir(ROOT "/books", 0755);
    mkdir(ROOT "/books/Series", 0755);
    mkdir(ROOT "/books/.hidden", 0755);
    mkdir(ROOT "/books/a", 0755);
    mkdir(ROOT "/books/a/b", 0755);
    mkdir(ROOT "/books/a/b/c", 0755);
    mkdir(ROOT "/books/a/b/c/d", 0755);
    writeFile(ROOT "/books/zebra_tales.epub", "x");
    writeFile(ROOT "/books/Atlas.PDF", "xx");
    writeFile(ROOT "/books/movie.mkv", "x");
    writeFile(ROOT "/books/.secret.epub", "x");
    writeFile(ROOT "/books/Series/one.cbz", "x");
    writeFile(ROOT "/books/.hidden/two.epub", "x");
    writeFile(ROOT "/books/a/b/c/deep.fb2", "x");     // Depth 4: found.
    writeFile(ROOT "/books/a/b/c/d/deeper.txt", "x");  // Depth 5: not.
}

static int indexOf(const Shelf *s, const char *name)
{
    for (int i = 0; i < s->count; i++) {
        const char *slash = strrchr(s->items[i].path, '/');
        if (slash && !strcmp(slash + 1, name))
            return i;
    }
    return -1;
}

static void testScan(void)
{
    makeTree();
    Shelf s = {0};
    CHECK(shelfLoad(&s, ROOT "/none.txt") && s.count == 0);
    CHECK(shelfScan(&s, ROOT "/books/") == 4);
    CHECK(indexOf(&s, "zebra_tales.epub") >= 0 && indexOf(&s, "Atlas.PDF") >= 0 && indexOf(&s, "one.cbz") >= 0);
    CHECK(indexOf(&s, "deep.fb2") >= 0 && indexOf(&s, "deeper.txt") < 0);
    CHECK(indexOf(&s, "movie.mkv") < 0 && indexOf(&s, ".secret.epub") < 0 && indexOf(&s, "two.epub") < 0);
    ShelfBook *z = shelfFind(&s, ROOT "/books/zebra_tales.epub");
    CHECK(z && z->size == 1 && z->percent == -1 && z->present);
    char name[SHELF_TEXT_MAX];
    if (z) {
        shelfDisplayName(z, name, sizeof(name));
        CHECK(!strcmp(name, "zebra tales"));
    }

    // Order: being read (most recent first), then by name.
    int order[16];
    int n = shelfOrder(&s, order, 16);
    CHECK(n == 4);
    if (n == 4) {
        shelfDisplayName(&s.items[order[0]], name, sizeof(name));
        CHECK(!strcmp(name, "Atlas"));
    }
    ShelfBook *deep = &s.items[indexOf(&s, "deep.fb2")];
    shelfMarkRead(deep, (DocPlace){2, 0.25}, 40, 1000);
    shelfMarkRead(z, (DocPlace){0, 0.5}, 150, 2000);
    CHECK(z->percent == 100);
    shelfSetInfo(z, "Zebra\tTales", "Ana\nExample");
    n = shelfOrder(&s, order, 16);
    CHECK(n == 4 && &s.items[order[0]] == z && &s.items[order[1]] == deep);
    shelfDisplayName(z, name, sizeof(name));
    CHECK(!strcmp(name, "Zebra Tales") && !strcmp(z->author, "Ana Example"));

    // Saved and read back.
    CHECK(shelfSave(&s, ROOT "/shelf.txt"));
    Shelf t = {0};
    CHECK(shelfLoad(&t, ROOT "/shelf.txt") && t.count == 4);
    ShelfBook *z2 = shelfFind(&t, ROOT "/books/zebra_tales.epub");
    CHECK(z2 && !z2->present && z2->percent == 100 && z2->last_read == 2000 && z2->place.fraction == 0.5);
    CHECK(z2 && !strcmp(z2->title, "Zebra Tales") && !strcmp(z2->author, "Ana Example"));
    ShelfBook *d2 = shelfFind(&t, deep->path);
    CHECK(d2 && d2->place.chapter == 2 && d2->place.fraction == 0.25);

    // A book removed is no longer present; one read is still remembered, one never read is forgotten on save.
    remove(ROOT "/books/zebra_tales.epub");
    remove(ROOT "/books/Atlas.PDF");
    CHECK(shelfScan(&t, ROOT "/books") == 2);
    CHECK(shelfOrder(&t, order, 16) == 2);
    CHECK(shelfSave(&t, ROOT "/shelf.txt"));
    shelfFree(&t);
    CHECK(shelfLoad(&t, ROOT "/shelf.txt") && t.count == 3);
    CHECK(shelfFind(&t, ROOT "/books/zebra_tales.epub") && !shelfFind(&t, ROOT "/books/Atlas.PDF"));

    // Another file under a read book's name: its place is forgotten.
    writeFile(ROOT "/books/zebra_tales.epub", "a new and longer file");
    shelfScan(&t, ROOT "/books");
    z2 = shelfFind(&t, ROOT "/books/zebra_tales.epub");
    CHECK(z2 && z2->present && z2->percent == -1 && z2->last_read == 0 && !z2->title[0]);
    shelfFree(&t);
    shelfFree(&s);
}

static void testBadFile(void)
{
    writeFile(ROOT "/bad.txt",
              "# comment\n"
              "\n"
              "only\tthree\tfields\n"
              "/b/x.epub\t5\t-3\t7.5\t900\t-4\tT\n"
              "/b/x.epub\t9\t1\t0.5\t50\t10\tDup\tDup\n"
              "/b/y.pdf\t1\t1\t0.5\t50\t10\n");
    Shelf s = {0};
    CHECK(shelfLoad(&s, ROOT "/bad.txt") && s.count == 2);
    ShelfBook *x = shelfFind(&s, "/b/x.epub");
    CHECK(x && x->place.chapter == 0 && x->place.fraction == 0 && x->percent == -1 && x->last_read == 0);
    CHECK(x && !strcmp(x->title, "T") && !x->author[0]);
    ShelfBook *y = shelfFind(&s, "/b/y.pdf");
    CHECK(y && y->percent == 50 && !y->title[0]);
    // A line far too long is skipped whole.
    FILE *f = fopen(ROOT "/long.txt", "w");
    if (f) {
        for (int i = 0; i < 5000; i++)
            fputc('a', f);
        fputs("\t1\t0\t0\t1\t1\n/ok.epub\t1\t0\t0\t1\t1\n", f);
        fclose(f);
    }
    shelfFree(&s);
    CHECK(shelfLoad(&s, ROOT "/long.txt") && s.count == 1 && shelfFind(&s, "/ok.epub"));
    // A title cut in the middle of a UTF-8 character drops the piece.
    ShelfBook *b = shelfAdd(&s, "/c.epub");
    char title[SHELF_TEXT_MAX + 8];
    memset(title, 'a', sizeof(title));
    memcpy(title + SHELF_TEXT_MAX - 2, "\xC3\xA9\xC3\xA9", 4);
    title[sizeof(title) - 1] = 0;
    shelfSetInfo(b, title, NULL);
    CHECK(strlen(b->title) == SHELF_TEXT_MAX - 2 && !b->author[0]);
    shelfFree(&s);
}

void testShelf(void)
{
    testScan();
    testBadFile();
}
