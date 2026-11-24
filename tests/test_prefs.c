// prefs.c: reading and writing the settings, and bad values left out.
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "check.h"
#include "gfx.h"
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
    p.two_pages = true;
    CHECK(prefsSave(&p, TMP_ROOT "/settings.txt"));
    Prefs q;
    prefsDefaults(&q);
    CHECK(prefsLoad(&q, TMP_ROOT "/settings.txt"));
    CHECK(q.text_size == 7 && q.theme == THEME_BLACK && !q.clock_24h && q.sort == SHELF_SORT_GENRE && q.turn == 2 && q.two_pages);

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

    // The quick turn: upright on the side used last (right at first), and lying down again.
    Prefs t;
    prefsDefaults(&t);
    CHECK(prefsToggleTurn(&t, GFX_TURN_NONE) == GFX_TURN_RIGHT && prefsToggleTurn(&t, GFX_TURN_RIGHT) == GFX_TURN_NONE);
    t.turn = GFX_TURN_LEFT;  // Chosen in Settings,
    CHECK(prefsToggleTurn(&t, GFX_TURN_LEFT) == GFX_TURN_NONE && t.upright == GFX_TURN_LEFT);
    CHECK(prefsToggleTurn(&t, GFX_TURN_NONE) == GFX_TURN_LEFT);  // and the quick turn comes back to it.
    CHECK(prefsSave(&t, TMP_ROOT "/turn.txt"));
    prefsDefaults(&q);
    CHECK(prefsLoad(&q, TMP_ROOT "/turn.txt") && q.upright == GFX_TURN_LEFT);

    // Automatic, with what the console's sensor gave in handheld (motion.log, 2026-11-24).
    CHECK(prefsTurnFromGravity(0.01f, -0.79f, -0.61f, GFX_TURN_RIGHT) == GFX_TURN_NONE);   // Held lying down, tilted.
    CHECK(prefsTurnFromGravity(1.01f, -0.04f, -0.05f, GFX_TURN_NONE) == GFX_TURN_RIGHT);   // Right side down.
    CHECK(prefsTurnFromGravity(-0.98f, -0.01f, -0.12f, GFX_TURN_NONE) == GFX_TURN_LEFT);   // Left side down.
    CHECK(prefsTurnFromGravity(0.66f, -0.45f, -0.07f, GFX_TURN_LEFT) == GFX_TURN_LEFT);    // Halfway: as it was.
    CHECK(prefsTurnFromGravity(0.03f, -0.24f, -0.96f, GFX_TURN_RIGHT) == GFX_TURN_RIGHT);  // Flat on a table: as it was.
    t.turn = PREFS_TURN_AUTO;  // The quick turn leaves Automatic, from how the screen is.
    CHECK(prefsToggleTurn(&t, GFX_TURN_NONE) == GFX_TURN_LEFT && t.turn == GFX_TURN_LEFT);
    t.turn = PREFS_TURN_AUTO;
    CHECK(prefsToggleTurn(&t, GFX_TURN_RIGHT) == GFX_TURN_NONE && t.upright == GFX_TURN_RIGHT);
    CHECK(!strcmp(prefsTurnName(PREFS_TURN_AUTO), "Automatic (handheld)"));
    t.turn = PREFS_TURN_AUTO;
    CHECK(prefsSave(&t, TMP_ROOT "/turn.txt") && prefsLoad(&q, TMP_ROOT "/turn.txt") && q.turn == PREFS_TURN_AUTO);

    CHECK(prefsEm(-5) == prefsEm(0) && prefsEm(99) == prefsEm(PREFS_TEXT_SIZES - 1));
    for (int i = 1; i < PREFS_TEXT_SIZES; i++)
        CHECK(prefsEm(i) > prefsEm(i - 1));
    CHECK(!strcmp(prefsThemeName(THEME_SEPIA), "Sepia") && !strcmp(prefsThemeName(42), "Light"));
    DocColors black = prefsColors(THEME_BLACK);
    CHECK(black.paper == 0 && black.ink != 0);
    CHECK(prefsColors(-1).paper == prefsColors(THEME_LIGHT).paper);
}
