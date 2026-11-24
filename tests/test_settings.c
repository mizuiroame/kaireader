// settings.c: the pages, their rows, and what a change does.
#include <string.h>

#include "check.h"
#include "gfx.h"
#include "settings.h"

void testSettings(void)
{
    SettingRow rows[SETTINGS_ROWS_MAX];
    CHECK(settingsRows(SETTINGS_PAGE_MAIN, rows) == 4 && rows[0] == SETTING_OPEN_READING);
    CHECK(settingsOpens(rows[3]) == SETTINGS_PAGE_ABOUT && settingsOpens(SETTING_CLOCK) == SETTINGS_PAGE_MAIN);
    CHECK(settingsRows(SETTINGS_PAGE_ABOUT, rows) == 0);
    for (int page = 0; page < SETTINGS_PAGE_COUNT; page++) {
        int n = settingsRows((SettingsPage)page, rows);
        CHECK(n <= SETTINGS_ROWS_MAX && settingsPageTitle((SettingsPage)page)[0]);
        for (int i = 0; i < n; i++)
            CHECK(settingsLabel(rows[i])[0] && settingsHelp(rows[i])[0]);
    }

    Prefs p;
    prefsDefaults(&p);
    char value[64];
    bool name;
    settingsValue(&p, SETTING_TEXT_SIZE, value, sizeof(value), &name);
    CHECK(!strcmp(value, "5 / 10") && !name);
    settingsValue(&p, SETTING_COLORS, value, sizeof(value), &name);
    CHECK(!strcmp(value, "Light") && name);
    settingsValue(&p, SETTING_RESCAN, value, sizeof(value), &name);
    CHECK(!value[0] && !settingsHasValue(SETTING_RESCAN) && settingsHasValue(SETTING_SCREEN));

    // Text size stops at its ends; the others go around.
    p.text_size = PREFS_TEXT_SIZES - 1;
    CHECK(settingsStep(&p, SETTING_TEXT_SIZE, 1) == SETTINGS_CHANGED_NOTHING && p.text_size == PREFS_TEXT_SIZES - 1);
    CHECK(settingsStep(&p, SETTING_TEXT_SIZE, -1) == SETTINGS_CHANGED_LAYOUT && p.text_size == PREFS_TEXT_SIZES - 2);
    CHECK(settingsStep(&p, SETTING_COLORS, -1) == SETTINGS_CHANGED_LAYOUT && p.theme == THEME_BLACK);
    CHECK(settingsStep(&p, SETTING_COLORS, 1) == SETTINGS_CHANGED_LAYOUT && p.theme == THEME_LIGHT);
    CHECK(settingsStep(&p, SETTING_SORT, -1) == SETTINGS_CHANGED_ORDER && p.sort == SHELF_SORT_GENRE);
    int changed = settingsStep(&p, SETTING_SCREEN, 1);
    CHECK((changed & SETTINGS_CHANGED_SCREEN) && (changed & SETTINGS_CHANGED_LAYOUT) && p.turn == GFX_TURN_RIGHT);
    settingsValue(&p, SETTING_SCREEN, value, sizeof(value), &name);
    CHECK(!strcmp(value, "Upright, right side down"));
    settingsStep(&p, SETTING_SCREEN, 1);
    CHECK(settingsStep(&p, SETTING_SCREEN, 1) && p.turn == GFX_TURN_NONE);
    CHECK(settingsStep(&p, SETTING_CLOCK, 1) == SETTINGS_CHANGED_NOTHING && !p.clock_24h);
    settingsValue(&p, SETTING_CLOCK, value, sizeof(value), &name);
    CHECK(!strcmp(value, "12-hour"));
    CHECK(settingsStep(&p, SETTING_RESCAN, 0) == SETTINGS_RESCAN && settingsStep(&p, SETTING_RESCAN, 1) == 0);
    CHECK(settingsStep(&p, SETTING_OPEN_READING, 1) == SETTINGS_CHANGED_NOTHING);
}
