// settings.c: the pages, their rows, and what a change does.
#include <string.h>

#include "check.h"
#include "gfx.h"
#include "settings.h"
#include "theme.h"

void testSettings(void)
{
    SettingRow rows[SETTINGS_ROWS_MAX];
    CHECK(settingsRows(SETTINGS_PAGE_MAIN, rows) == 7 && rows[0] == SETTING_OPEN_LIBRARY);
    CHECK(settingsOpens(rows[1]) == SETTINGS_PAGE_NETWORK && settingsOpens(rows[2]) == SETTINGS_PAGE_SERVER);
    CHECK(settingsOpens(rows[3]) == SETTINGS_PAGE_NEWS && settingsPickedValue(rows[3]));
    CHECK(settingsOpens(rows[4]) == SETTINGS_PAGE_READING && settingsOpens(rows[6]) == SETTINGS_PAGE_ABOUT);
    {  // The news' page: its feeds' rows are the app's, its color a setting; how many feeds on the main page.
        SettingRow news[SETTINGS_ROWS_MAX];
        int n = settingsRows(SETTINGS_PAGE_NEWS, news);
        CHECK(n == 8 && news[0] == SETTING_NEWS_ADD && news[1] == SETTING_OPEN_NEWS_SUGGESTED && news[n - 1] == SETTING_NEWS_REMOVE);
        CHECK(settingsIsNews(SETTING_NEWS_FEED) && !settingsIsNews(SETTING_NEWS_COLOR) &&
              settingsHasValue(SETTING_NEWS_FEED) && !settingsHasValue(SETTING_NEWS_ADD));
        Prefs p;
        prefsDefaults(&p);
        char v[64];
        bool named;
        settingsSetNews(0, "", true);
        settingsValue(&p, SETTING_OPEN_NEWS, v, sizeof(v), &named);
        CHECK(!strcmp(v, "Not set") && named);
        settingsSetNews(3, "Harbor", false);
        settingsValue(&p, SETTING_OPEN_NEWS, v, sizeof(v), &named);
        CHECK(!strcmp(v, "3 feeds") && !named);
        settingsValue(&p, SETTING_NEWS_FEED, v, sizeof(v), &named);
        CHECK(!strcmp(v, "Harbor") && !named);
        settingsValue(&p, SETTING_NEWS_CERTIFICATE, v, sizeof(v), &named);
        CHECK(!strcmp(v, "Off: any certificate"));
        CHECK(settingsStep(&p, SETTING_NEWS_COLOR, 1) == SETTINGS_CHANGED_SOURCE_COLOR && p.news_color == 1);
        CHECK(settingsOpens(SETTING_OPEN_NEWS) == SETTINGS_PAGE_NEWS);
        settingsSetNews(0, "", true);
    }
    // A source's page, as the other Kai apps': address, sign-in, options (its color last), test, forget.
    CHECK(settingsRows(SETTINGS_PAGE_SERVER, rows) == 6 && rows[0] == SETTING_SRV_ADDRESS && settingsIsServer(rows[5]));
    CHECK(rows[2] == SETTING_SRV_CERTIFICATE && !settingsIsServer(rows[2]) && settingsHasValue(rows[2]));
    CHECK(rows[3] == SETTING_SRV_COLOR && settingsHasValue(rows[3]) && rows[4] == SETTING_SRV_TEST);
    {  // A source's color: its own, then each accent, kept.
        Prefs colors;
        prefsDefaults(&colors);
        char v[64];
        bool named;
        settingsValue(&colors, SETTING_NET_COLOR, v, sizeof(v), &named);
        CHECK(!strcmp(v, "Its own") && named);
        CHECK(settingsStep(&colors, SETTING_NET_COLOR, 1) == SETTINGS_CHANGED_SOURCE_COLOR && colors.network_color == 1);
        settingsValue(&colors, SETTING_NET_COLOR, v, sizeof(v), &named);
        CHECK(!strcmp(v, "Amber"));
        CHECK(settingsStep(&colors, SETTING_SRV_COLOR, -1) == SETTINGS_CHANGED_SOURCE_COLOR &&
              colors.server_color == THEME_ACCENT_COUNT);
    }
    Prefs cert;
    prefsDefaults(&cert);
    CHECK(settingsStep(&cert, SETTING_SRV_CERTIFICATE, 1) == SETTINGS_CHANGED_CERTIFICATE && cert.server_any_certificate);
    CHECK(settingsStep(&cert, SETTING_RESUME, 1) == SETTINGS_CHANGED_NOTHING && !cert.resume);
    CHECK(settingsStep(&cert, SETTING_UPDATE_AUTO, 1) == SETTINGS_CHANGED_NOTHING && !cert.update_check);
    CHECK(!settingsIsServer(SETTING_NET_TEST) && !settingsIsNetwork(SETTING_SRV_TEST) && settingsPickedValue(SETTING_SRV_ADDRESS));
    CHECK(settingsOpens(SETTING_CLOCK) == SETTINGS_PAGE_MAIN);
    // Display: the language first, its name as given (in itself, not translated).
    CHECK(settingsRows(SETTINGS_PAGE_DISPLAY, rows) == 6 && rows[0] == SETTING_LANGUAGE && settingsHasValue(SETTING_LANGUAGE));
    CHECK(rows[1] == SETTING_ACCENT && rows[2] == SETTING_BACKGROUND && rows[3] == SETTING_BLUR);
    {
        Prefs lk;
        prefsDefaults(&lk);
        char v[64];
        bool named;
        CHECK(settingsStep(&lk, SETTING_ACCENT, -1) == SETTINGS_CHANGED_ACCENT && lk.accent == THEME_ACCENT_COUNT - 1);
        settingsValue(&lk, SETTING_ACCENT, v, sizeof(v), &named);
        CHECK(!strcmp(v, "Red") && named);
        settingsValue(&lk, SETTING_BACKGROUND, v, sizeof(v), &named);
        CHECK(!strcmp(v, "None") && named);
        snprintf(lk.background, sizeof(lk.background), "%s", "lake.jpg");
        settingsValue(&lk, SETTING_BACKGROUND, v, sizeof(v), &named);
        CHECK(!strcmp(v, "lake.jpg") && !named);
        CHECK(settingsStep(&lk, SETTING_BLUR, 1) == SETTINGS_CHANGED_BLUR && lk.background_blur == 3);
        settingsValue(&lk, SETTING_BLUR, v, sizeof(v), &named);
        CHECK(!strcmp(v, "High"));
    }
    {
        Prefs lp;
        prefsDefaults(&lp);
        char v[64];
        bool named = true;
        settingsSetLanguageName("Deutsch");
        settingsValue(&lp, SETTING_LANGUAGE, v, sizeof(v), &named);
        CHECK(!named && !strcmp(v, "Deutsch"));
        settingsSetLanguageName("English");
    }
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
    // Address, sign-in, Remember, signing, Certificate check, its color, test, forget.
    CHECK(settingsRows(SETTINGS_PAGE_NETWORK, rows) == 8 && rows[0] == SETTING_NET_ADDRESS && rows[1] == SETTING_NET_ACCOUNT &&
          rows[2] == SETTING_NET_REMEMBER && rows[3] == SETTING_NET_UNSIGNED && rows[4] == SETTING_NET_CERTIFICATE &&
          rows[5] == SETTING_NET_COLOR && rows[6] == SETTING_NET_TEST && rows[7] == SETTING_NET_FORGET);
    CHECK(settingsIsNetwork(SETTING_NET_CERTIFICATE) && settingsHasValue(SETTING_NET_CERTIFICATE) &&
          !settingsIsServer(SETTING_NET_CERTIFICATE));
    CHECK(!strcmp(settingsLabel(SETTING_NET_CERTIFICATE), settingsLabel(SETTING_SRV_CERTIFICATE)));
    CHECK(settingsIsNetwork(SETTING_NET_TEST) && !settingsIsNetwork(SETTING_OPEN_NETWORK) && !settingsIsNetwork(SETTING_CLOCK));
    CHECK(settingsPickedValue(SETTING_NET_ADDRESS) && !settingsPickedValue(SETTING_NET_REMEMBER));
    {
        char v[64];
        bool named;
        Prefs none;
        prefsDefaults(&none);
        settingsSetNetwork("", "", true, false, false, true);
        settingsValue(&none, SETTING_NET_ADDRESS, v, sizeof(v), &named);
        CHECK(named && strcmp(v, "Not set") == 0);
        settingsValue(&none, SETTING_NET_CERTIFICATE, v, sizeof(v), &named);
        CHECK(named && strcmp(v, "On") == 0);  // Checked unless turned off.
        settingsSetNetwork("https://nas/dav/", "reader", true, false, false, false);
        settingsValue(&none, SETTING_NET_CERTIFICATE, v, sizeof(v), &named);
        CHECK(named && strcmp(v, "Off: any certificate") == 0);
        settingsSetNetwork("smb://NAS/Books", "reader", false, true, true, true);
        settingsValue(&none, SETTING_NET_ADDRESS, v, sizeof(v), &named);
        CHECK(!named && strcmp(v, "smb://NAS/Books") == 0);
        settingsValue(&none, SETTING_NET_REMEMBER, v, sizeof(v), &named);
        CHECK(named && strncmp(v, "Off", 3) == 0);
        settingsValue(&none, SETTING_NET_UNSIGNED, v, sizeof(v), &named);
        CHECK(strcmp(v, "On: not signed") == 0);
        settingsSetNetwork("", "", true, false, false, true);
    }
    CHECK(settingsRows(SETTINGS_PAGE_ABOUT, rows) == 3 && settingsOpens(rows[0]) == SETTINGS_PAGE_CREDITS);
    CHECK(rows[1] == SETTING_UPDATE_AUTO && settingsPickedValue(rows[2]) && settingsRows(SETTINGS_PAGE_CREDITS, rows) == 0);
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
