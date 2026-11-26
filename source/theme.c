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
#define NETWORK_OWN GFX_RGB(0x2E, 0xC4, 0xB6)  // Turquoise, as in KaiPlayer and KaiVideo.
#define SERVER_OWN GFX_RGB(0x3D, 0x6B, 0xFF)   // The deep blue of KaiPlayer's server.
#define NEWS_OWN GFX_RGB(0xFF, 0x86, 0x5E)     // Coral, the news'.
Color g_network_color = NETWORK_OWN, g_server_color = SERVER_OWN, g_news_color = NEWS_OWN;

void themeApplySources(int network, int server, int news)
{
    g_news_color = news > 0 && news <= THEME_ACCENT_COUNT ? COLORS[news - 1] : NEWS_OWN;
    g_network_color = network > 0 && network <= THEME_ACCENT_COUNT ? COLORS[network - 1] : NETWORK_OWN;
    g_server_color = server > 0 && server <= THEME_ACCENT_COUNT ? COLORS[server - 1] : SERVER_OWN;
}

const char *themeAccentName(int accent)
{
    return accent >= 0 && accent < THEME_ACCENT_COUNT ? NAMES[accent] : NAMES[0];
}

void themeApply(int accent)
{
    g_accent = accent >= 0 && accent < THEME_ACCENT_COUNT ? COLORS[accent] : COLORS[0];
}
