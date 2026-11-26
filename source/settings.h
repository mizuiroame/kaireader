// Settings, in pages as in KaiRadio: the main page opens Reading, Library,
// Display and About; each row shows a value changed with left and right (or
// a tap), and a help line. Pure logic over Prefs, tested on the PC; ui.c
// draws it and app.c follows what a change needs.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "prefs.h"

typedef enum {
    SETTINGS_PAGE_MAIN,
    SETTINGS_PAGE_READING,
    SETTINGS_PAGE_LIBRARY,
    SETTINGS_PAGE_DISPLAY,
    SETTINGS_PAGE_ABOUT,
    SETTINGS_PAGE_NETWORK,
    SETTINGS_PAGE_SERVER,
    SETTINGS_PAGE_CREDITS,  // About's: who made it, and their licenses.
    SETTINGS_PAGE_NEWS,     // The news feeds followed (app_news.c does its rows).
    SETTINGS_PAGE_COUNT,
} SettingsPage;

typedef enum {
    SETTING_OPEN_READING,  // The main page's rows: they open a page.
    SETTING_OPEN_LIBRARY,
    SETTING_OPEN_DISPLAY,
    SETTING_OPEN_ABOUT,
    SETTING_TEXT_SIZE,
    SETTING_COLORS,
    SETTING_PAGES,         // One page, or two side by side.
    SETTING_SORT,
    SETTING_VIEW,          // The library as a grid or a list.
    SETTING_RESCAN,        // Looks for books again (an action, no value).
    SETTING_SCREEN,
    SETTING_CLOCK,
    SETTING_OPEN_NETWORK,  // The network folder's page (app_network.c does its rows).
    SETTING_NET_ADDRESS,
    SETTING_NET_ACCOUNT,
    SETTING_NET_REMEMBER,
    SETTING_NET_UNSIGNED,
    SETTING_NET_TEST,
    SETTING_NET_FORGET,
    SETTING_OPEN_SERVER,   // The book server's page (app_server.c does its rows).
    SETTING_SRV_ADDRESS,
    SETTING_SRV_ACCOUNT,
    SETTING_SRV_TEST,
    SETTING_SRV_FORGET,
    SETTING_LANGUAGE,      // The app's language (the app steps it: languages.h).
    SETTING_RESUME,        // The last book opened again at start.
    SETTING_SRV_CERTIFICATE,  // The book server's certificate checked, or any taken (its own).
    SETTING_CREDITS,       // About: opens Credits and licenses.
    SETTING_UPDATE_AUTO,   // Look for a new version once a day.
    SETTING_UPDATE_NOW,    // Look now, or install what was found (app_update.c shows its value).
    SETTING_ACCENT,        // The accent color.
    SETTING_BACKGROUND,    // An image behind the library (the app steps it: the folder's files).
    SETTING_BLUR,          // How soft that image is.
    SETTING_NET_COLOR,     // The color the network folder wears: its own, or an accent.
    SETTING_SRV_COLOR,     // The book server's.
    SETTING_NET_CERTIFICATE,  // The network folder's https certificate checked, or any taken (its own).
    SETTING_OPEN_NEWS,     // The news' page (app_news.c does its rows).
    SETTING_NEWS_ADD,      // A feed's address, or a page linking to it (the keyboard).
    SETTING_NEWS_IMPORT,   // The feeds of feeds.opml.
    SETTING_NEWS_FEED,     // The feed the rows under it are about (stepped).
    SETTING_NEWS_RENAME,
    SETTING_NEWS_REMOVE,
    SETTING_NEWS_CERTIFICATE,  // That feed's https certificate checked, or any taken.
    SETTING_NEWS_COLOR,    // The news' color.
    SETTING_COUNT,
} SettingRow;

#define SETTINGS_ROWS_MAX 8

// What a change needs from the app.
typedef enum {
    SETTINGS_CHANGED_NOTHING = 0,
    SETTINGS_CHANGED_LAYOUT = 1,  // The book open is laid out again (text size, colors, screen).
    SETTINGS_CHANGED_ORDER = 2,   // The library is sorted again.
    SETTINGS_CHANGED_SCREEN = 4,  // The screen turned.
    SETTINGS_RESCAN = 8,          // Look for books.
    SETTINGS_CHANGED_CERTIFICATE = 16,  // The book server's certificate check.
    SETTINGS_CHANGED_ACCENT = 32,       // The accent color.
    SETTINGS_CHANGED_BLUR = 64,         // The background made again.
    SETTINGS_CHANGED_SOURCE_COLOR = 128,  // A source's color.
} SettingsChange;

// The rows of a page, into out; how many.
int settingsRows(SettingsPage page, SettingRow *out);
const char *settingsPageTitle(SettingsPage page);  // English, shown with T().
const char *settingsLabel(SettingRow row);
const char *settingsHelp(SettingRow row);
// The page a main-page row opens, or SETTINGS_PAGE_MAIN for the rest.
SettingsPage settingsOpens(SettingRow row);
// True for a row with a value (left and right change it).
bool settingsHasValue(SettingRow row);
// The value shown, in English for T() when names ("Sepia"), or a number ("5 of 10").
// translate_name is true when out holds a name to pass through T().
void settingsValue(const Prefs *p, SettingRow row, char *out, size_t size, bool *translate_name);
// The network folder's rows are the app's (app_network.c): true for them.
bool settingsIsNetwork(SettingRow row);
// The book server's rows are the app's too (app_server.c).
bool settingsIsServer(SettingRow row);
// The news' rows are the app's too (app_news.c); its color is a setting.
bool settingsIsNews(SettingRow row);
// What the news' rows show: how many feeds, the one chosen's name and its certificate check.
void settingsSetNews(int count, const char *feed_name, bool check_certificate);
// What the book server's rows show: its address as shown, and the user.
void settingsSetServer(const char *address, const char *user);
// The language in use's name, as its row shows it ("Español (Latinoamérica)").
void settingsSetLanguageName(const char *name);
// A row that shows a value but is picked, not stepped (the folder's address): no arrows.
bool settingsPickedValue(SettingRow row);
// What the network folder's rows show: its address as shown, the user, the two switches,
// and whether it's a shared folder (SMB: signing applies).
void settingsSetNetwork(const char *address, const char *user, bool remember, bool allow_unsigned, bool smb,
                        bool check_certificate);
// Steps a row's value back (-1) or forward (+1), around at the ends; an
// action row (Look for books again) is done with dir 0. What changed.
int settingsStep(Prefs *p, SettingRow row, int dir);
