// settings.c: the pages, their rows, and what a change does.
#include <string.h>

#include "check.h"
#include "gfx.h"
#include "settings.h"

void testSettings(void)
{
    SettingRow rows[SETTINGS_ROWS_MAX];
    CHECK(settingsRows(SETTINGS_PAGE_MAIN, rows) == 6 && rows[0] == SETTING_OPEN_LIBRARY);
    CHECK(settingsOpens(rows[1]) == SETTINGS_PAGE_NETWORK && settingsOpens(rows[2]) == SETTINGS_PAGE_SERVER);
    CHECK(settingsOpens(rows[3]) == SETTINGS_PAGE_READING && settingsOpens(rows[5]) == SETTINGS_PAGE_ABOUT);
    CHECK(settingsRows(SETTINGS_PAGE_SERVER, rows) == 4 && rows[0] == SETTING_SRV_ADDRESS && settingsIsServer(rows[3]));
    CHECK(!settingsIsServer(SETTING_NET_TEST) && !settingsIsNetwork(SETTING_SRV_TEST) && settingsPickedValue(SETTING_SRV_ADDRESS));
    CHECK(settingsOpens(SETTING_CLOCK) == SETTINGS_PAGE_MAIN);
    // Library: the sort, the view (a grid or a list), looking again.
    CHECK(settingsRows(SETTINGS_PAGE_LIBRARY, rows) == 3 && rows[1] == SETTING_VIEW && settingsHasValue(SETTING_VIEW));
    {
        Prefs v;
        prefsDefaults(&v);
        CHECK(!v.list_view && settingsStep(&v, SETTING_VIEW, 1) == SETTINGS_CHANGED_ORDER && v.list_view);
        // Y goes Recent, Favorites, Title, Author, Genre, and round.
        v.sort = SHELF_SORT_RECENT;
        settingsStep(&v, SETTING_SORT, 1);
        CHECK(v.sort == SHELF_SORT_FAVORITES);
        settingsStep(&v, SETTING_SORT, 1);
        CHECK(v.sort == SHELF_SORT_TITLE);
        settingsStep(&v, SETTING_SORT, -1);
        settingsStep(&v, SETTING_SORT, -1);
        CHECK(v.sort == SHELF_SORT_RECENT);
    }
    // The network folder's page: its rows are the app's, the address shown as set.
    CHECK(settingsRows(SETTINGS_PAGE_NETWORK, rows) == 6 && rows[0] == SETTING_NET_ADDRESS && rows[5] == SETTING_NET_FORGET);
    CHECK(settingsIsNetwork(SETTING_NET_TEST) && !settingsIsNetwork(SETTING_OPEN_NETWORK) && !settingsIsNetwork(SETTING_CLOCK));
    CHECK(settingsPickedValue(SETTING_NET_ADDRESS) && !settingsPickedValue(SETTING_NET_REMEMBER));
    {
        char v[64];
        bool named;
        Prefs none;
        prefsDefaults(&none);
        settingsSetNetwork("", "", true, false, false);
        settingsValue(&none, SETTING_NET_ADDRESS, v, sizeof(v), &named);
        CHECK(named && strcmp(v, "Not set") == 0);
        settingsSetNetwork("smb://NAS/Books", "reader", false, true, true);
        settingsValue(&none, SETTING_NET_ADDRESS, v, sizeof(v), &named);
        CHECK(!named && strcmp(v, "smb://NAS/Books") == 0);
        settingsValue(&none, SETTING_NET_REMEMBER, v, sizeof(v), &named);
        CHECK(named && strncmp(v, "Off", 3) == 0);
        settingsValue(&none, SETTING_NET_UNSIGNED, v, sizeof(v), &named);
        CHECK(strcmp(v, "On: not signed") == 0);
        settingsSetNetwork("", "", true, false, false);
    }
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
    CHECK(!strcmp(value, "Vertical"));
    CHECK(settingsStep(&p, SETTING_SCREEN, 1) && p.turn == PREFS_TURN_AUTO);  // Horizontal, Vertical, Automatic,
    CHECK(settingsStep(&p, SETTING_SCREEN, 1) && p.turn == GFX_TURN_NONE);    // and round again.
    CHECK(settingsStep(&p, SETTING_PAGES, 1) == SETTINGS_CHANGED_LAYOUT && p.two_pages);
    settingsValue(&p, SETTING_PAGES, value, sizeof(value), &name);
    CHECK(!strcmp(value, "Two side by side") && name);
    CHECK(settingsStep(&p, SETTING_CLOCK, 1) == SETTINGS_CHANGED_NOTHING && !p.clock_24h);
    settingsValue(&p, SETTING_CLOCK, value, sizeof(value), &name);
    CHECK(!strcmp(value, "12-hour"));
    CHECK(settingsStep(&p, SETTING_RESCAN, 0) == SETTINGS_RESCAN && settingsStep(&p, SETTING_RESCAN, 1) == 0);
    CHECK(settingsStep(&p, SETTING_OPEN_READING, 1) == SETTINGS_CHANGED_NOTHING);
}
