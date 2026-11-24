#include "screen.h"

#include <switch.h>

#define FADE_NS 300000000ull

static bool g_has_lbl;
static bool g_on = true;
static bool g_backlight_off;  // We turned the console's backlight off.

void screenInit(void)
{
    g_has_lbl = R_SUCCEEDED(lblInitialize());
}

void screenExit(void)
{
    screenSetOn(true);
    if (g_has_lbl)
        lblExit();
    g_has_lbl = false;
}

// Docked, the picture is on the TV: the console's own backlight is the
// system's business, and turning it on would light the screen in the dock.
static bool docked(void)
{
    return appletGetOperationMode() == AppletOperationMode_Console;
}

void screenSetOn(bool on)
{
    if (on == g_on)
        return;
    g_on = on;
    if (!on && g_has_lbl && !docked()) {
        lblSwitchBacklightOff(FADE_NS);
        g_backlight_off = true;
    } else if (on && g_backlight_off) {
        // Only what we turned off, and only in handheld mode; after docking,
        // the system decides.
        if (!docked())
            lblSwitchBacklightOn(FADE_NS);
        g_backlight_off = false;
    }
}

bool screenIsOn(void)
{
    return g_on;
}

bool screenHasBacklightControl(void)
{
    return g_has_lbl && !docked();
}
