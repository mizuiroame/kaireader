#include "prefs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "lang.h"

static const float EMS[PREFS_TEXT_SIZES] = {15, 17, 19, 21, 23, 25, 28, 31, 35, 40};

static const char *const THEME_NAMES[THEME_COUNT] = {
    N_("Light"),
    N_("Sepia"),
    N_("Dark"),
    N_("Black"),
};

static const char *const SORT_NAMES[SHELF_SORT_COUNT] = {
    N_("Recently read"),
    N_("Title"),
    N_("Author"),
    N_("Genre"),
};

static const char *const TURN_NAMES[3] = {
    N_("Lying down"),
    N_("Upright, right side down"),
    N_("Upright, left side down"),
};

// Ink and paper, 0xRRGGBB: soft contrast, never pure black on pure white.
static const DocColors THEME_COLORS[THEME_COUNT] = {
    {0x1E1E1E, 0xFAF8F3},
    {0x40321F, 0xF2E6CE},
    {0xD6D4CF, 0x1B1C1F},
    {0xA9A7A2, 0x000000},
};

void prefsDefaults(Prefs *p)
{
    p->text_size = PREFS_TEXT_DEFAULT;
    p->theme = THEME_LIGHT;
    p->clock_24h = true;
    p->sort = SHELF_SORT_RECENT;
    p->turn = GFX_TURN_NONE;
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
    return s;
}

// A whole number from min to max; false (and nothing set) for anything else.
static bool number(const char *text, int min, int max, int *out)
{
    char *end;
    long v = strtol(text, &end, 10);
    if (end == text || *end || v < min || v > max)
        return false;
    *out = (int)v;
    return true;
}

bool prefsLoad(Prefs *p, const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f)
        return errno == ENOENT;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq)
            continue;
        *eq = 0;
        const char *key = trim(line), *value = trim(eq + 1);
        int v;
        if (!strcmp(key, "text_size") && number(value, 0, PREFS_TEXT_SIZES - 1, &v))
            p->text_size = v;
        else if (!strcmp(key, "theme") && number(value, 0, THEME_COUNT - 1, &v))
            p->theme = v;
        else if (!strcmp(key, "clock_24h") && number(value, 0, 1, &v))
            p->clock_24h = v != 0;
        else if (!strcmp(key, "sort") && number(value, 0, SHELF_SORT_COUNT - 1, &v))
            p->sort = v;
        else if (!strcmp(key, "turn") && number(value, 0, GFX_TURN_LEFT, &v))
            p->turn = v;
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool prefsSave(const Prefs *p, const char *file)
{
    char tmp[600];
    if (snprintf(tmp, sizeof(tmp), "%s.new", file) >= (int)sizeof(tmp))
        return false;
    FILE *f = fopen(tmp, "w");
    if (!f)
        return false;
    fprintf(f, "# KaiReader settings\ntext_size = %d\ntheme = %d\nclock_24h = %d\nsort = %d\nturn = %d\n",
            p->text_size, p->theme, p->clock_24h ? 1 : 0, p->sort, p->turn);
    bool ok = !ferror(f);
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp);
        return false;
    }
    remove(file);  // newlib's rename doesn't replace a file on the SD.
    return rename(tmp, file) == 0;
}

float prefsEm(int text_size)
{
    if (text_size < 0)
        text_size = 0;
    if (text_size >= PREFS_TEXT_SIZES)
        text_size = PREFS_TEXT_SIZES - 1;
    return EMS[text_size];
}

const char *prefsThemeName(int theme)
{
    return theme >= 0 && theme < THEME_COUNT ? THEME_NAMES[theme] : THEME_NAMES[0];
}

const char *prefsSortName(int sort)
{
    return sort >= 0 && sort < SHELF_SORT_COUNT ? SORT_NAMES[sort] : SORT_NAMES[0];
}

const char *prefsTurnName(int turn)
{
    return turn >= 0 && turn <= GFX_TURN_LEFT ? TURN_NAMES[turn] : TURN_NAMES[0];
}

DocColors prefsColors(int theme)
{
    return theme >= 0 && theme < THEME_COUNT ? THEME_COLORS[theme] : THEME_COLORS[0];
}
