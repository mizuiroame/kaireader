// How books are shown: the text size and the colors of the page, and the
// clock's format. Kept in settings.txt next to the NRO ("key = value" lines).
#pragma once

#include <stdbool.h>

#include "doc.h"

#define PREFS_TEXT_SIZES 10
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
} Prefs;

void prefsDefaults(Prefs *p);
// Reads the file over the defaults; unknown keys and bad values are skipped. False if it's there and unreadable.
bool prefsLoad(Prefs *p, const char *file);
bool prefsSave(const Prefs *p, const char *file);

// The text size in points of a size index (clamped).
float prefsEm(int text_size);
const char *prefsThemeName(int theme);  // English: shown with T().
DocColors prefsColors(int theme);
