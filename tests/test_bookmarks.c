// bookmarks.c: adding, finding, ordering, and the file read back (bad lines left out).
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "bookmarks.h"
#include "check.h"

void testBookmarks(void)
{
    mkdir(TMP_ROOT, 0755);
    Bookmarks b = {0};
    const char *book = "sdmc:/books/Quiet.epub", *other = "sdmc:/books/Other.pdf";
    CHECK(bookmarksAdd(&b, book, (DocPlace){2, 0.5}, 100));
    CHECK(bookmarksAdd(&b, book, (DocPlace){0, 0.25}, 101));
    CHECK(bookmarksAdd(&b, other, (DocPlace){0, 0.1}, 102));
    CHECK(bookmarksAdd(&b, book, (DocPlace){2, 0.125}, 103));
    CHECK(!bookmarksAdd(&b, book, (DocPlace){2, 0.5}, 104));  // There already.
    CHECK(!bookmarksAdd(&b, "", (DocPlace){0, 0}, 1) && !bookmarksAdd(&b, "a\tb", (DocPlace){0, 0}, 1));
    CHECK(!bookmarksAdd(&b, book, (DocPlace){0, 1.5}, 1) && !bookmarksAdd(&b, book, (DocPlace){-1, 0}, 1));

    int mine[8];
    int n = bookmarksOf(&b, book, mine, 8);
    CHECK(n == 3);  // In reading order: chapter 0, then chapter 2's two.
    if (n == 3) {
        CHECK(b.items[mine[0]].place.chapter == 0 && b.items[mine[1]].place.fraction == 0.125);
        CHECK(b.items[mine[2]].place.fraction == 0.5);
    }
    CHECK(bookmarksFind(&b, book, (DocPlace){2, 0.5}) >= 0 && bookmarksFind(&b, book, (DocPlace){2, 0.51}) < 0);
    CHECK(bookmarksFind(&b, other, (DocPlace){2, 0.5}) < 0);

    CHECK(bookmarksSave(&b, TMP_ROOT "/bookmarks.txt"));
    Bookmarks back = {0};
    CHECK(bookmarksLoad(&back, TMP_ROOT "/bookmarks.txt") && back.count == 4);
    CHECK(bookmarksFind(&back, book, (DocPlace){2, 0.125}) >= 0 && back.items[0].made == 100);

    bookmarksRemove(&b, bookmarksFind(&b, book, (DocPlace){0, 0.25}));
    CHECK(bookmarksOf(&b, book, mine, 8) == 2 && b.count == 3);
    bookmarksRemove(&b, -1);
    bookmarksRemove(&b, 99);
    CHECK(b.count == 3);

    // A book holds at most BOOKMARKS_PER_BOOK.
    Bookmarks many = {0};
    for (int i = 0; i < BOOKMARKS_PER_BOOK; i++)
        CHECK(bookmarksAdd(&many, book, (DocPlace){i, 0}, i));
    CHECK(!bookmarksAdd(&many, book, (DocPlace){999, 0}, 0) && bookmarksAdd(&many, other, (DocPlace){0, 0}, 0));

    FILE *f = fopen(TMP_ROOT "/bad-bookmarks.txt", "w");
    if (f) {
        fputs("# c\nsdmc:/a.epub\t1\t0.5\t7\nno tabs\nsdmc:/a.epub\tx\t0.5\t7\nsdmc:/a.epub\t1\t2\t7\n"
              "sdmc:/a.epub\t1\t0.5\t7\nsdmc:/a.epub\t1\t0.5\nsdmc:/b.epub\t3\t0.75\t-1\n\tsdmc\t1\t0\t0\n",
              f);
        fclose(f);
    }
    Bookmarks bad = {0};
    CHECK(bookmarksLoad(&bad, TMP_ROOT "/bad-bookmarks.txt") && bad.count == 1);  // Only the first good line, once.
    CHECK(bookmarksLoad(&bad, TMP_ROOT "/none.txt") && bad.count == 0);

    bookmarksFree(&b);
    bookmarksFree(&back);
    bookmarksFree(&many);
    bookmarksFree(&bad);
}
