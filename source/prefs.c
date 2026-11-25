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
    N_("Favorites"),
};

static const char *const TURN_NAMES[4] = {
    N_("Horizontal"),
    N_("Vertical"),
    N_("Vertical"),  // Either side down: the side is the quick turn's (or Automatic's) to choose.
    N_("Automatic"),
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
    p->upright = GFX_TURN_RIGHT;
    p->two_pages = false;
    p->list_view = false;
    p->welcomed = false;
    p->resume = true;
    p->last_tab = 0;
    p->last_book[0] = '\0';
    p->server_any_certificate = false;
    p->update_check = true;
    p->accent = 0;
    p->background[0] = '\0';
    p->background_blur = 2;  // Medium.
    p->update_checked = 0;
    p->update_tag[0] = '\0';
    snprintf(p->language, sizeof(p->language), "%s", "en");
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
        else if (!strcmp(key, "turn") && number(value, 0, PREFS_TURN_AUTO, &v))
            p->turn = v;
        else if (!strcmp(key, "upright") && number(value, GFX_TURN_RIGHT, GFX_TURN_LEFT, &v))
            p->upright = v;
        else if (!strcmp(key, "two_pages") && number(value, 0, 1, &v))
            p->two_pages = v != 0;
        else if (!strcmp(key, "list_view") && number(value, 0, 1, &v))
            p->list_view = v != 0;
        else if (!strcmp(key, "welcomed") && number(value, 0, 1, &v))
            p->welcomed = v != 0;
        else if (!strcmp(key, "resume") && number(value, 0, 1, &v))
            p->resume = v != 0;
        else if (!strcmp(key, "last_tab") && number(value, 0, 3, &v))
            p->last_tab = v;
        else if (!strcmp(key, "last_book") && strlen(value) < sizeof(p->last_book) && !strpbrk(value, "\r\n"))
            snprintf(p->last_book, sizeof(p->last_book), "%s", value);
        else if (!strcmp(key, "server_any_certificate") && number(value, 0, 1, &v))
            p->server_any_certificate = v != 0;
        else if (!strcmp(key, "accent") && number(value, 0, 5, &v))
            p->accent = v;
        else if (!strcmp(key, "background_blur") && number(value, 0, 3, &v))
            p->background_blur = v;
        else if (!strcmp(key, "background") && strlen(value) < sizeof(p->background) && !strpbrk(value, "/\\\r\n"))
            snprintf(p->background, sizeof(p->background), "%s", value);  // A file's name: never a path.
        else if (!strcmp(key, "update_check") && number(value, 0, 1, &v))
            p->update_check = v != 0;
        else if (!strcmp(key, "update_checked")) {
            char *end;
            long long t = strtoll(value, &end, 10);
            if (end != value && !*end && t >= 0)
                p->update_checked = t;
        } else if (!strcmp(key, "update_tag") && strlen(value) < sizeof(p->update_tag) && !strpbrk(value, "/\\\r\n"))
            snprintf(p->update_tag, sizeof(p->update_tag), "%s", value);
        else if (!strcmp(key, "language") && value[0] && strlen(value) < sizeof(p->language) &&
                 !strpbrk(value, "/\\\r\n"))
            snprintf(p->language, sizeof(p->language), "%s", value);  // An id or a file's name: never a path.
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
    fprintf(f, "# KaiReader settings\ntext_size = %d\ntheme = %d\nclock_24h = %d\nsort = %d\nturn = %d\n"
               "two_pages = %d\n",
            p->text_size, p->theme, p->clock_24h ? 1 : 0, p->sort, p->turn, p->two_pages ? 1 : 0);
    fprintf(f, "upright = %d\nlist_view = %d\nlanguage = %s\nwelcomed = %d\n", p->upright, p->list_view ? 1 : 0, p->language,
            p->welcomed ? 1 : 0);
    fprintf(f, "resume = %d\nlast_tab = %d\nlast_book = %s\nserver_any_certificate = %d\n", p->resume ? 1 : 0,
            p->last_tab, p->last_book, p->server_any_certificate ? 1 : 0);
    fprintf(f, "accent = %d\nbackground = %s\nbackground_blur = %d\n", p->accent, p->background, p->background_blur);
    fprintf(f, "update_check = %d\nupdate_checked = %lld\nupdate_tag = %s\n", p->update_check ? 1 : 0, p->update_checked,
            p->update_tag);
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

#define LEAN 0.75f    // How much of gravity along a side, at least, to call it that side down,
#define MARGIN 0.35f  // and how much more than along the other side.

int prefsTurnFromGravity(float x, float y, float z, int current)
{
    (void)z;  // Lying flat on a table (z about -1): no side is down.
    float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if (ax >= LEAN && ax > ay + MARGIN)
        return x > 0 ? GFX_TURN_RIGHT : GFX_TURN_LEFT;
    if (y <= -0.6f && ay > ax + MARGIN)
        return GFX_TURN_NONE;
    return current;
}

int prefsStepTurn(Prefs *p, int dir)
{
    // The three choices in order; Vertical is whichever side it stood on last.
    int at = p->turn == GFX_TURN_NONE ? 0 : p->turn == PREFS_TURN_AUTO ? 2 : 1;
    if (p->turn == GFX_TURN_RIGHT || p->turn == GFX_TURN_LEFT)
        p->upright = p->turn;
    at = ((at + (dir < 0 ? -1 : 1)) % 3 + 3) % 3;
    p->turn = at == 0 ? GFX_TURN_NONE : at == 2 ? PREFS_TURN_AUTO : p->upright == GFX_TURN_LEFT ? GFX_TURN_LEFT
                                                                                                 : GFX_TURN_RIGHT;
    return p->turn;
}

int prefsToggleTurn(Prefs *p, int current)
{
    if (p->turn == PREFS_TURN_AUTO)
        p->turn = current;
    if (p->turn != GFX_TURN_NONE) {
        p->upright = p->turn;  // Remembered for standing it up again.
        p->turn = GFX_TURN_NONE;
    } else {
        p->turn = p->upright == GFX_TURN_LEFT ? GFX_TURN_LEFT : GFX_TURN_RIGHT;
    }
    return p->turn;
}

const char *prefsTurnName(int turn)
{
    return turn >= 0 && turn <= PREFS_TURN_AUTO ? TURN_NAMES[turn] : TURN_NAMES[0];
}

DocColors prefsColors(int theme)
{
    return theme >= 0 && theme < THEME_COUNT ? THEME_COLORS[theme] : THEME_COLORS[0];
}
