// How books are shown: the text size and the colors of the page, and the
// clock's format. Kept in settings.txt next to the NRO ("key = value" lines).
#pragma once

#include <stdbool.h>

#include "doc.h"
#include "shelf.h"

#define PREFS_TEXT_SIZES 10
#define PREFS_TURN_AUTO 3  // Prefs.turn: turning with the console (handheld), after GFX_TURN_LEFT.
#define PREFS_TEXT_DEFAULT 4

typedef enum {
    THEME_LIGHT,
    THEME_SEPIA,
    THEME_DARK,
    THEME_BLACK,  // Black paper, for OLED screens and reading in bed.
    THEME_COUNT,
} ReadTheme;

typedef struct {
    int text_size;  // 0 to PREFS_TEXT_SIZES - 1.
    int theme;      // ReadTheme.
    bool clock_24h;
    int sort;       // ShelfSort: how the library is ordered.
    int turn;       // gfx.h's GFX_TURN_*: the console held as it is, or upright like a book; or PREFS_TURN_AUTO.
    int upright;    // GFX_TURN_RIGHT or _LEFT: the side the quick button stands it on (the last one used).
    bool two_pages; // Two pages side by side, like an open book (with the console lying down).
    bool list_view; // The library as a list (small covers, more words) instead of a grid.
    char language[64]; // Its id (languages.h): "en", "es", or a file in the lang folder.
} Prefs;

void prefsDefaults(Prefs *p);
// Reads the file over the defaults; unknown keys and bad values are skipped. False if it's there and unreadable.
bool prefsLoad(Prefs *p, const char *file);
bool prefsSave(const Prefs *p, const char *file);

// The text size in points of a size index (clamped).
float prefsEm(int text_size);
const char *prefsThemeName(int theme);  // English: shown with T().
const char *prefsSortName(int sort);
const char *prefsTurnName(int turn);
// Settings' Screen: Horizontal, Vertical (on the side used last), Automatic, and round.
int prefsStepTurn(Prefs *p, int dir);
// The quick button's turn: lying down if upright, else upright on the side used last.
// From Automatic, it starts from current (how the screen is now) and leaves Automatic.
int prefsToggleTurn(Prefs *p, int current);
// Automatic: the turn gravity asks for (x, y, z in g, as the console's motion
// sensor gives them in handheld: lying down y is about -1, right side down x
// about +1). A clear lean only; anything in between, or lying flat, keeps current.
int prefsTurnFromGravity(float x, float y, float z, int current);
DocColors prefsColors(int theme);
