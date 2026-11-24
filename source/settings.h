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
    SETTING_COUNT,
} SettingRow;

#define SETTINGS_ROWS_MAX 6

// What a change needs from the app.
typedef enum {
    SETTINGS_CHANGED_NOTHING = 0,
    SETTINGS_CHANGED_LAYOUT = 1,  // The book open is laid out again (text size, colors, screen).
    SETTINGS_CHANGED_ORDER = 2,   // The library is sorted again.
    SETTINGS_CHANGED_SCREEN = 4,  // The screen turned.
    SETTINGS_RESCAN = 8,          // Look for books.
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
// A row that shows a value but is picked, not stepped (the folder's address): no arrows.
bool settingsPickedValue(SettingRow row);
// What the network folder's rows show: its address as shown, the user, the two switches,
// and whether it's a shared folder (SMB: signing applies).
void settingsSetNetwork(const char *address, const char *user, bool remember, bool allow_unsigned, bool smb);
// Steps a row's value back (-1) or forward (+1), around at the ends; an
// action row (Look for books again) is done with dir 0. What changed.
int settingsStep(Prefs *p, SettingRow row, int dir);
