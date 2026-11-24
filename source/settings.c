#include "settings.h"

#include <stdio.h>

#include "gfx.h"
#include "lang.h"

static const char *const PAGE_TITLES[SETTINGS_PAGE_COUNT] = {
    N_("Settings"),
    N_("Reading"),
    N_("Library"),
    N_("Display"),
    N_("About KaiReader"),
};

static const char *const LABELS[SETTING_COUNT] = {
    N_("Reading"),
    N_("Library"),
    N_("Display"),
    N_("About KaiReader"),
    N_("Text size"),
    N_("Colors"),
    N_("Pages"),
    N_("Sort books by"),
    N_("Look for books again"),
    N_("Screen"),
    N_("Clock"),
};

static const char *const HELP[SETTING_COUNT] = {
    N_("Text size and the colors of the page."),
    N_("How the books are sorted, and looking for new ones."),
    N_("Reading with the console upright like a book, and the clock."),
    N_("The version, and the licenses of the parts KaiReader is made of."),
    N_("For books that flow (EPUB, FB2, MOBI, TXT). Up and down change it while reading."),
    N_("Light, sepia, dark or black paper, for books that flow. Y changes it while reading."),
    N_("Two pages side by side, like an open book, with the console lying down. Upright, always one."),
    N_("Recently read, by title, by author or by genre (from the book's subject). Y changes it in the library."),
    N_("Looks in sdmc:/books again for books added or removed."),
    N_("Vertical: hold the console like a book. Automatic: it turns as you turn the console (handheld). The right stick pressed in switches horizontal and vertical. The buttons and the touch screen turn with it."),
    N_("21:47 or 9:47 PM."),
};

int settingsRows(SettingsPage page, SettingRow *out)
{
    int n = 0;
    switch (page) {
    case SETTINGS_PAGE_MAIN:
        out[n++] = SETTING_OPEN_READING;
        out[n++] = SETTING_OPEN_LIBRARY;
        out[n++] = SETTING_OPEN_DISPLAY;
        out[n++] = SETTING_OPEN_ABOUT;
        break;
    case SETTINGS_PAGE_READING:
        out[n++] = SETTING_TEXT_SIZE;
        out[n++] = SETTING_COLORS;
        out[n++] = SETTING_PAGES;
        break;
    case SETTINGS_PAGE_LIBRARY:
        out[n++] = SETTING_SORT;
        out[n++] = SETTING_RESCAN;
        break;
    case SETTINGS_PAGE_DISPLAY:
        out[n++] = SETTING_SCREEN;
        out[n++] = SETTING_CLOCK;
        break;
    default:
        break;
    }
    return n;
}

const char *settingsPageTitle(SettingsPage page)
{
    return page >= 0 && page < SETTINGS_PAGE_COUNT ? PAGE_TITLES[page] : PAGE_TITLES[0];
}

const char *settingsLabel(SettingRow row)
{
    return row >= 0 && row < SETTING_COUNT ? LABELS[row] : "";
}

const char *settingsHelp(SettingRow row)
{
    return row >= 0 && row < SETTING_COUNT ? HELP[row] : "";
}

SettingsPage settingsOpens(SettingRow row)
{
    switch (row) {
    case SETTING_OPEN_READING:
        return SETTINGS_PAGE_READING;
    case SETTING_OPEN_LIBRARY:
        return SETTINGS_PAGE_LIBRARY;
    case SETTING_OPEN_DISPLAY:
        return SETTINGS_PAGE_DISPLAY;
    case SETTING_OPEN_ABOUT:
        return SETTINGS_PAGE_ABOUT;
    default:
        return SETTINGS_PAGE_MAIN;
    }
}

bool settingsHasValue(SettingRow row)
{
    return row == SETTING_TEXT_SIZE || row == SETTING_COLORS || row == SETTING_PAGES || row == SETTING_SORT ||
           row == SETTING_SCREEN || row == SETTING_CLOCK;
}

void settingsValue(const Prefs *p, SettingRow row, char *out, size_t size, bool *translate_name)
{
    *translate_name = true;
    switch (row) {
    case SETTING_TEXT_SIZE:
        *translate_name = false;
        snprintf(out, size, "%d / %d", p->text_size + 1, PREFS_TEXT_SIZES);
        break;
    case SETTING_COLORS:
        snprintf(out, size, "%s", prefsThemeName(p->theme));
        break;
    case SETTING_PAGES:
        snprintf(out, size, "%s", p->two_pages ? N_("Two side by side") : N_("One"));
        break;
    case SETTING_SORT:
        snprintf(out, size, "%s", prefsSortName(p->sort));
        break;
    case SETTING_SCREEN:
        snprintf(out, size, "%s", prefsTurnName(p->turn));
        break;
    case SETTING_CLOCK:
        snprintf(out, size, "%s", p->clock_24h ? N_("24-hour") : N_("12-hour"));
        break;
    default:
        *translate_name = false;
        if (size)
            out[0] = 0;
        break;
    }
}

// value + dir, around in 0..count-1.
static int around(int value, int dir, int count)
{
    return ((value + dir) % count + count) % count;
}

int settingsStep(Prefs *p, SettingRow row, int dir)
{
    switch (row) {
    case SETTING_TEXT_SIZE: {
        int size = p->text_size + (dir ? dir : 1);
        if (size < 0 || size >= PREFS_TEXT_SIZES)  // Stops at the ends: around would jump from tiny to huge.
            return SETTINGS_CHANGED_NOTHING;
        p->text_size = size;
        return SETTINGS_CHANGED_LAYOUT;
    }
    case SETTING_COLORS:
        p->theme = around(p->theme, dir ? dir : 1, THEME_COUNT);
        return SETTINGS_CHANGED_LAYOUT;
    case SETTING_PAGES:
        p->two_pages = !p->two_pages;
        return SETTINGS_CHANGED_LAYOUT;
    case SETTING_SORT:
        p->sort = around(p->sort, dir ? dir : 1, SHELF_SORT_COUNT);
        return SETTINGS_CHANGED_ORDER;
    case SETTING_SCREEN:
        prefsStepTurn(p, dir ? dir : 1);
        return SETTINGS_CHANGED_SCREEN | SETTINGS_CHANGED_LAYOUT;
    case SETTING_CLOCK:
        p->clock_24h = !p->clock_24h;
        return SETTINGS_CHANGED_NOTHING;
    case SETTING_RESCAN:
        return dir ? SETTINGS_CHANGED_NOTHING : SETTINGS_RESCAN;
    default:
        return SETTINGS_CHANGED_NOTHING;
    }
}
