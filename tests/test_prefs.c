// prefs.c: reading and writing the settings, and bad values left out.
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "check.h"
#include "prefs.h"

void testPrefs(void)
{
    mkdir(TMP_ROOT, 0755);
    Prefs p;
    prefsDefaults(&p);
    CHECK(p.text_size == PREFS_TEXT_DEFAULT && p.theme == THEME_LIGHT && p.clock_24h);
    CHECK(prefsLoad(&p, TMP_ROOT "/no-settings.txt") && p.text_size == PREFS_TEXT_DEFAULT);

    p.text_size = 7;
    p.theme = THEME_BLACK;
    p.clock_24h = false;
    p.sort = SHELF_SORT_GENRE;
    p.turn = 2;
    CHECK(prefsSave(&p, TMP_ROOT "/settings.txt"));
    Prefs q;
    prefsDefaults(&q);
    CHECK(prefsLoad(&q, TMP_ROOT "/settings.txt"));
    CHECK(q.text_size == 7 && q.theme == THEME_BLACK && !q.clock_24h && q.sort == SHELF_SORT_GENRE && q.turn == 2);

    FILE *f = fopen(TMP_ROOT "/bad-settings.txt", "w");
    if (f) {
        fputs("# c\ntext_size = 99\ntheme=2\r\nclock_24h = yes\nnothing\n = 3\ntheme = 1x\nsort = 9\nturn = -1\n", f);
        fclose(f);
    }
    prefsDefaults(&q);
    CHECK(prefsLoad(&q, TMP_ROOT "/bad-settings.txt"));
    CHECK(q.text_size == PREFS_TEXT_DEFAULT && q.theme == THEME_DARK && q.clock_24h && q.sort == 0 && q.turn == 0);
    CHECK(!strcmp(prefsSortName(SHELF_SORT_AUTHOR), "Author") && !strcmp(prefsSortName(99), "Recently read"));
    CHECK(!strcmp(prefsTurnName(1), "Upright, right side down") && !strcmp(prefsTurnName(7), "Lying down"));

    CHECK(prefsEm(-5) == prefsEm(0) && prefsEm(99) == prefsEm(PREFS_TEXT_SIZES - 1));
    for (int i = 1; i < PREFS_TEXT_SIZES; i++)
        CHECK(prefsEm(i) > prefsEm(i - 1));
    CHECK(!strcmp(prefsThemeName(THEME_SEPIA), "Sepia") && !strcmp(prefsThemeName(42), "Light"));
    DocColors black = prefsColors(THEME_BLACK);
    CHECK(black.paper == 0 && black.ink != 0);
    CHECK(prefsColors(-1).paper == prefsColors(THEME_LIGHT).paper);
}
