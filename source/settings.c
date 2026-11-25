#include "settings.h"

#include <stdio.h>

#include "gfx.h"
#include "lang.h"
#include "theme.h"

static const char *const PAGE_TITLES[SETTINGS_PAGE_COUNT] = {
    N_("Settings"),
    N_("Reading"),
    N_("Library"),
    N_("Display"),
    N_("About KaiReader"),
    N_("Network folder"),
    N_("Book server"),
    N_("Credits and licenses"),
};

static const char *const LABELS[SETTING_COUNT] = {
    N_("Reading"),
    N_("Library"),
    N_("Display"),
    N_("About"),
    N_("Text size"),
    N_("Colors"),
    N_("Pages"),
    N_("Sort books by"),
    N_("Show books as"),
    N_("Look for books again"),
    N_("Screen"),
    N_("Clock"),
    N_("Network folder"),
    N_("Address"),
    N_("User and password"),
    N_("Remember the password"),
    N_("Allow without signing (old NAS)"),
    N_("Test the connection"),
    N_("Forget this folder"),
    N_("Book server"),
    N_("Address"),
    N_("User and password"),
    N_("Test the connection"),
    N_("Forget this server"),
    N_("Language"),
    N_("Resume the last book"),
    N_("Certificate check"),
    N_("Credits and licenses"),
    N_("Check once a day"),
    N_("Look for an update"),
    N_("Accent color"),
    N_("Background"),
    N_("Background blur"),
    N_("Color"),
    N_("Color"),
};

static const char *const HELP[SETTING_COUNT] = {
    N_("Text size and the colors of the page."),
    N_("How the books are sorted, and looking for new ones."),
    N_("The language, reading with the console upright like a book, and the clock."),
    N_("The version, and the licenses of the parts KaiReader is made of."),
    N_("For books that flow (EPUB, FB2, MOBI, TXT). Up and down change it while reading."),
    N_("Light, sepia, dark or black paper, for books that flow. Y changes it while reading."),
    N_("Two pages side by side, like an open book, with the console lying down. Upright, always one."),
    N_("Recently read, your favorites, by title, by author or by genre (from the book's subject). Y changes it in the library."),
    N_("A grid of covers, or a list with more of each book. The + menu changes it too."),
    N_("Looks in sdmc:/books again for books added or removed."),
    N_("Vertical: hold the console like a book. Automatic: it turns as you turn the console (handheld). The right stick pressed in switches horizontal and vertical. The buttons and the touch screen turn with it."),
    N_("21:47 or 9:47 PM."),
    N_("A shared folder (SMB), WebDAV, FTP or SFTP folder on your network: X in the library switches between its books and the SD card's."),
    N_("\\\\PC\\Books (a Windows shared folder), 192.168.0.10:8098 (WebDAV or a web folder), ftp://nas or sftp://nas."),
    N_("Empty for a folder open to everyone."),
    N_("Off: the password isn't kept on the card, and it's asked for when the folder is used."),
    N_("Only for an old NAS or a folder open to guests that doesn't sign: someone on your network could change what's read."),
    N_("Asks the folder for its top: whether it answers, and how many folders and books it has."),
    N_("Its address, user and password are taken off the card. The books already copied stay."),
    N_("Calibre-Web, Kavita, Komga or another OPDS catalog on your network: X in the library goes to its books."),
    N_("192.168.0.10:8083 for Calibre-Web, 192.168.0.10:25600 for Komga (its /opds address is found), or a whole OPDS address (Kavita's, with its key)."),
    N_("Empty for a catalog open to everyone."),
    N_("Asks the server for its catalog: whether it answers, and what's at its start."),
    N_("Its address, user and password are taken off the card. The books already downloaded stay."),
    N_("The language of KaiReader's words (not of your books). More can be added as files in the lang folder."),
    N_("Opens the book you were reading, at its page, when KaiReader starts."),
    N_("Off only for a server of your own with a certificate it made itself (https). The server's sign-in still only goes to it."),
    N_("Who made KaiReader and what it's built with, and their licenses."),
    N_("Looks on GitHub for a new KaiReader once a day, when it opens, and says so."),
    N_("Looks now. In sdmc:/switch, A installs what it found; from Brewstore, Brewstore updates it."),
    N_("Highlights, buttons and the selected book."),
    N_("JPEG or PNG images in the Backgrounds folder of KaiReader's folder (sdmc:/switch/kaireader/config), behind the library."),
    N_("How soft the background image is."),
    N_("The network folder's dot and its library's color: its own (turquoise) or one of the accents."),
    N_("The book server's dot and its library's color: its own (blue) or one of the accents."),
};

static char g_language_name[64] = "English";

void settingsSetLanguageName(const char *name)
{
    snprintf(g_language_name, sizeof(g_language_name), "%s", name);
}

static char g_srv_address[256], g_srv_user[128];

void settingsSetServer(const char *address, const char *user)
{
    snprintf(g_srv_address, sizeof(g_srv_address), "%s", address);
    snprintf(g_srv_user, sizeof(g_srv_user), "%s", user);
}

bool settingsIsServer(SettingRow row)
{
    return row >= SETTING_SRV_ADDRESS && row <= SETTING_SRV_FORGET;  // Its account's rows (not its certificate's).
}

static char g_net_address[256], g_net_user[128];
static bool g_net_remember = true, g_net_unsigned, g_net_smb;

void settingsSetNetwork(const char *address, const char *user, bool remember, bool allow_unsigned, bool smb)
{
    snprintf(g_net_address, sizeof(g_net_address), "%s", address);
    snprintf(g_net_user, sizeof(g_net_user), "%s", user);
    g_net_remember = remember;
    g_net_unsigned = allow_unsigned;
    g_net_smb = smb;
}

bool settingsIsNetwork(SettingRow row)
{
    return row >= SETTING_NET_ADDRESS && row <= SETTING_NET_FORGET;
}

bool settingsPickedValue(SettingRow row)
{
    return row == SETTING_UPDATE_NOW || row == SETTING_OPEN_NETWORK || row == SETTING_NET_ADDRESS || row == SETTING_NET_ACCOUNT ||
           row == SETTING_OPEN_SERVER || row == SETTING_SRV_ADDRESS || row == SETTING_SRV_ACCOUNT;
}

int settingsRows(SettingsPage page, SettingRow *out)
{
    int n = 0;
    switch (page) {
    case SETTINGS_PAGE_MAIN:
        out[n++] = SETTING_OPEN_LIBRARY;  // As KaiPlayer's: the library, where it comes from, then the rest.
        out[n++] = SETTING_OPEN_NETWORK;
        out[n++] = SETTING_OPEN_SERVER;
        out[n++] = SETTING_OPEN_READING;
        out[n++] = SETTING_OPEN_DISPLAY;
        out[n++] = SETTING_OPEN_ABOUT;
        break;
    case SETTINGS_PAGE_READING:
        out[n++] = SETTING_TEXT_SIZE;
        out[n++] = SETTING_COLORS;
        out[n++] = SETTING_PAGES;
        out[n++] = SETTING_RESUME;
        break;
    case SETTINGS_PAGE_LIBRARY:
        out[n++] = SETTING_SORT;
        out[n++] = SETTING_VIEW;
        out[n++] = SETTING_RESCAN;
        break;
    case SETTINGS_PAGE_DISPLAY:
        out[n++] = SETTING_LANGUAGE;
        out[n++] = SETTING_ACCENT;
        out[n++] = SETTING_BACKGROUND;
        out[n++] = SETTING_BLUR;
        out[n++] = SETTING_CLOCK;
        out[n++] = SETTING_SCREEN;  // KaiReader's own, last.
        break;
    case SETTINGS_PAGE_NETWORK:
        out[n++] = SETTING_NET_ADDRESS;
        out[n++] = SETTING_NET_ACCOUNT;
        out[n++] = SETTING_NET_REMEMBER;
        out[n++] = SETTING_NET_UNSIGNED;
        out[n++] = SETTING_NET_COLOR;
        out[n++] = SETTING_NET_TEST;
        out[n++] = SETTING_NET_FORGET;
        break;
    case SETTINGS_PAGE_SERVER:
        out[n++] = SETTING_SRV_ADDRESS;
        out[n++] = SETTING_SRV_ACCOUNT;
        out[n++] = SETTING_SRV_CERTIFICATE;
        out[n++] = SETTING_SRV_COLOR;
        out[n++] = SETTING_SRV_TEST;
        out[n++] = SETTING_SRV_FORGET;
        break;
    case SETTINGS_PAGE_ABOUT:
        out[n++] = SETTING_CREDITS;
        out[n++] = SETTING_UPDATE_AUTO;
        out[n++] = SETTING_UPDATE_NOW;
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
    case SETTING_CREDITS:
        return SETTINGS_PAGE_CREDITS;
    case SETTING_OPEN_NETWORK:
        return SETTINGS_PAGE_NETWORK;
    case SETTING_OPEN_SERVER:
        return SETTINGS_PAGE_SERVER;
    default:
        return SETTINGS_PAGE_MAIN;
    }
}

bool settingsHasValue(SettingRow row)
{
    return row == SETTING_TEXT_SIZE || row == SETTING_COLORS || row == SETTING_PAGES || row == SETTING_SORT ||
           row == SETTING_VIEW || row == SETTING_LANGUAGE || row == SETTING_RESUME || row == SETTING_SRV_CERTIFICATE || row == SETTING_UPDATE_AUTO || row == SETTING_ACCENT ||
           row == SETTING_BACKGROUND || row == SETTING_BLUR || row == SETTING_NET_COLOR || row == SETTING_SRV_COLOR ||
           row == SETTING_SCREEN || row == SETTING_CLOCK || row == SETTING_NET_REMEMBER || row == SETTING_NET_UNSIGNED ||
           settingsPickedValue(row);
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
    case SETTING_VIEW:
        snprintf(out, size, "%s", p->list_view ? N_("A list") : N_("A grid"));
        break;
    case SETTING_RESUME:
        snprintf(out, size, "%s", p->resume ? N_("On") : N_("Off"));
        break;
    case SETTING_UPDATE_AUTO:
        snprintf(out, size, "%s", p->update_check ? N_("On") : N_("Off"));
        break;
    case SETTING_ACCENT:
        snprintf(out, size, "%s", themeAccentName(p->accent));
        break;
    case SETTING_NET_COLOR:
    case SETTING_SRV_COLOR: {
        int choice = row == SETTING_NET_COLOR ? p->network_color : p->server_color;
        snprintf(out, size, "%s", choice ? themeAccentName(choice - 1) : N_("Its own"));
        break;
    }
    case SETTING_BACKGROUND:  // A file's name, as it is; or None.
        *translate_name = !p->background[0];
        snprintf(out, size, "%s", p->background[0] ? p->background : N_("None"));
        break;
    case SETTING_BLUR: {
        static const char *const BLURS[4] = {N_("Off"), N_("Low"), N_("Medium"), N_("High")};
        snprintf(out, size, "%s", BLURS[p->background_blur >= 0 && p->background_blur < 4 ? p->background_blur : 2]);
        break;
    }
    case SETTING_SRV_CERTIFICATE:
        snprintf(out, size, "%s", p->server_any_certificate ? N_("Off: any certificate") : N_("On"));
        break;
    case SETTING_LANGUAGE:  // Its own name, in itself: not translated.
        *translate_name = false;
        snprintf(out, size, "%s", g_language_name);
        break;
    case SETTING_SCREEN:
        snprintf(out, size, "%s", prefsTurnName(p->turn));
        break;
    case SETTING_CLOCK:
        snprintf(out, size, "%s", p->clock_24h ? N_("24-hour") : N_("12-hour"));
        break;
    case SETTING_OPEN_NETWORK:
    case SETTING_NET_ADDRESS:
        *translate_name = g_net_address[0] == 0;
        snprintf(out, size, "%s", g_net_address[0] ? g_net_address : N_("Not set"));
        break;
    case SETTING_NET_ACCOUNT:
        *translate_name = g_net_user[0] == 0;
        snprintf(out, size, "%s", g_net_user[0] ? g_net_user : N_("None"));
        break;
    case SETTING_OPEN_SERVER:
    case SETTING_SRV_ADDRESS:
        *translate_name = g_srv_address[0] == 0;
        snprintf(out, size, "%s", g_srv_address[0] ? g_srv_address : N_("Not set"));
        break;
    case SETTING_SRV_ACCOUNT:
        *translate_name = g_srv_user[0] == 0;
        snprintf(out, size, "%s", g_srv_user[0] ? g_srv_user : N_("None"));
        break;
    case SETTING_NET_REMEMBER:
        snprintf(out, size, "%s", g_net_remember ? N_("On") : N_("Off: asked when the folder is used"));
        break;
    case SETTING_NET_UNSIGNED:
        snprintf(out, size, "%s", !g_net_smb ? N_("Only for shared folders (SMB)") : g_net_unsigned ? N_("On: not signed") : N_("Off"));
        break;
    default:
        *translate_name = false;
        if (size)
            out[0] = 0;
        break;
    }
}

// The sorts in the order Y and the arrows go through them: Favorites after Recent.
static const int SORT_STEPS[SHELF_SORT_COUNT] = {SHELF_SORT_RECENT, SHELF_SORT_FAVORITES, SHELF_SORT_TITLE,
                                                 SHELF_SORT_AUTHOR, SHELF_SORT_GENRE};

static int stepSort(int sort, int dir)
{
    int at = 0;
    for (int i = 0; i < SHELF_SORT_COUNT; i++)
        if (SORT_STEPS[i] == sort)
            at = i;
    return SORT_STEPS[((at + dir) % SHELF_SORT_COUNT + SHELF_SORT_COUNT) % SHELF_SORT_COUNT];
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
        p->sort = stepSort(p->sort, dir ? dir : 1);
        return SETTINGS_CHANGED_ORDER;
    case SETTING_VIEW:
        p->list_view = !p->list_view;
        return SETTINGS_CHANGED_ORDER;  // Laid out again.
    case SETTING_SCREEN:
        prefsStepTurn(p, dir ? dir : 1);
        return SETTINGS_CHANGED_SCREEN | SETTINGS_CHANGED_LAYOUT;
    case SETTING_CLOCK:
        p->clock_24h = !p->clock_24h;
        return SETTINGS_CHANGED_NOTHING;
    case SETTING_RESUME:
        p->resume = !p->resume;
        return SETTINGS_CHANGED_NOTHING;
    case SETTING_UPDATE_AUTO:
        p->update_check = !p->update_check;
        return SETTINGS_CHANGED_NOTHING;
    case SETTING_ACCENT:
        p->accent = around(p->accent, dir, THEME_ACCENT_COUNT);
        return SETTINGS_CHANGED_ACCENT;
    case SETTING_BLUR:
        p->background_blur = around(p->background_blur, dir, 4);
        return SETTINGS_CHANGED_BLUR;
    case SETTING_NET_COLOR:
        p->network_color = around(p->network_color, dir, THEME_ACCENT_COUNT + 1);
        return SETTINGS_CHANGED_SOURCE_COLOR;
    case SETTING_SRV_COLOR:
        p->server_color = around(p->server_color, dir, THEME_ACCENT_COUNT + 1);
        return SETTINGS_CHANGED_SOURCE_COLOR;
    case SETTING_SRV_CERTIFICATE:
        p->server_any_certificate = !p->server_any_certificate;
        return SETTINGS_CHANGED_CERTIFICATE;
    case SETTING_RESCAN:
        return dir ? SETTINGS_CHANGED_NOTHING : SETTINGS_RESCAN;
    default:
        return SETTINGS_CHANGED_NOTHING;
    }
}
