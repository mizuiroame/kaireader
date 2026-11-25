#include "theme.h"

#include "lang.h"

// KaiReader's amber first (its own); then KaiPlayer's.
static const char *const NAMES[THEME_ACCENT_COUNT] = {
    N_("Amber"), N_("Pink"), N_("Blue"), N_("Green"), N_("Purple"), N_("Red"),
};
static const Color COLORS[THEME_ACCENT_COUNT] = {
    GFX_RGB(0xF2, 0xB5, 0x4A), GFX_RGB(0xFF, 0x6B, 0x9A), GFX_RGB(0x4D, 0xA3, 0xFF),
    GFX_RGB(0x3D, 0xD6, 0x8C), GFX_RGB(0xA7, 0x8B, 0xFA), GFX_RGB(0xFF, 0x5C, 0x5C),
};

Color g_accent = GFX_RGB(0xF2, 0xB5, 0x4A);

const char *themeAccentName(int accent)
{
    return accent >= 0 && accent < THEME_ACCENT_COUNT ? NAMES[accent] : NAMES[0];
}

void themeApply(int accent)
{
    g_accent = accent >= 0 && accent < THEME_ACCENT_COUNT ? COLORS[accent] : COLORS[0];
}
