// The news' showcase (ui_news.c): its lead story and rows of wide cards, drawn
// line by line by the library's drawing (ui.c).
#pragma once

#include <stddef.h>

#include "app.h"

// A line of the showcase at y (on screen), inside the library's view (view_top to view_bottom).
void uiNewsDrawLine(App *app, const LibLine *line, int y, int view_top, int view_bottom);
// The size an article's picture is asked for at (every view of it, one size).
void uiNewsCoverSize(int *w, int *h);
// When an article came, as the news says it: "2 h ago", "Yesterday", or its day.
void uiNewsWhen(const ShelfBook *b, char *out, size_t size);
