// The accent color, chosen in Settings > Display, as in KaiPlayer: highlights,
// the selected row, buttons. ui_common.h's COLOR_ACCENT reads g_accent.
#pragma once

#include "gfx.h"

#define THEME_ACCENT_COUNT 6

extern Color g_accent;

// An accent's name, in English for T() (out of range: the first).
const char *themeAccentName(int accent);
// Sets g_accent (out of range: the first, amber).
void themeApply(int accent);
// The network folder's, the book server's and the news' colors: 0 their own, else an accent + 1.
extern Color g_network_color, g_server_color, g_news_color;
void themeApplySources(int network, int server, int news);
