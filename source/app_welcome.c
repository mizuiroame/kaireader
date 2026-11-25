// The first run's welcome, as KaiPlayer's: where are the books? On the SD
// card (how to put them there), a network folder or a book server (their
// Settings pages, back here with B), Free books, or later. It shows once, while
// the card has no books and nothing else is set up; nothing has to be chosen.
#include "app.h"

#include "buttons.h"
#include "davnet.h"
#include "lang.h"
#include "opdsnet.h"

static bool sourceSet(void)
{
    return davConfigured() || opdsConfigured();
}

bool appWelcomeNeeded(const App *app)
{
    return !app->prefs.welcomed && app->order_count == 0 && app->lib_tab == LIB_CARD && !sourceSet();
}

void appOpenWelcome(App *app)
{
    app->panel = PANEL_WELCOME;
    app->welcome_away = false;
    if (app->welcome_cursor < 0 || app->welcome_cursor >= WELCOME_ROW_COUNT)
        app->welcome_cursor = WELCOME_CARD;
}

static void answered(App *app)
{
    if (app->panel == PANEL_WELCOME)
        app->panel = PANEL_NONE;
    app->welcome_away = false;
    app->settings_from_welcome = false;
    app->prefs.welcomed = true;
    appSavePrefs(app);
}

void appWelcomeChoose(App *app, int row, uint64_t now_ns)
{
    app->welcome_cursor = row;
    switch (row) {
    case WELCOME_CARD:
        answered(app);
        appNotify(app, NOTICE_INFO,
                  T("Copy your books to sdmc:/books (KaiDrop gives them names the console can list), then choose \"Look "
                    "for books again\" in the + menu."),
                  now_ns);
        break;
    case WELCOME_NETWORK:
    case WELCOME_SERVER:  // Its Settings page; back here with B if it isn't set up.
        appOpenSettingsPage(app, row == WELCOME_NETWORK ? SETTINGS_PAGE_NETWORK : SETTINGS_PAGE_SERVER);
        app->welcome_away = true;
        app->settings_from_welcome = true;
        break;
    case WELCOME_FREE:
        answered(app);
        appLibTab(app, LIB_FREE, now_ns);
        break;
    case WELCOME_LATER:
        answered(app);
        break;
    default:
        break;
    }
}

void appWelcomeButtons(App *app, uint64_t pressed, uint64_t now_ns)
{
    if ((pressed & BUTTONS_UP) && app->welcome_cursor > 0)
        app->welcome_cursor--;
    else if ((pressed & BUTTONS_DOWN) && app->welcome_cursor + 1 < WELCOME_ROW_COUNT)
        app->welcome_cursor++;
    else if (pressed & HidNpadButton_A)
        appWelcomeChoose(app, app->welcome_cursor, now_ns);
    else if (pressed & (HidNpadButton_B | HidNpadButton_Plus))
        appWelcomeChoose(app, WELCOME_LATER, now_ns);  // B answers "later", as the row does.
}

void appWelcomeTick(App *app)
{
    if (!app->welcome_away || app->panel == PANEL_SETTINGS)
        return;
    if (sourceSet())  // A folder or a server set up: nothing more to ask.
        answered(app);
    else
        appOpenWelcome(app);  // Left without one: choose again.
}
