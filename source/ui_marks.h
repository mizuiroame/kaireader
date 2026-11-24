// Bookmarks on screen (ui_marks.c).
#pragma once

#include "app.h"

#define MARK_BUTTON_W 56

// The ribbon at the top of a bookmarked page (each of two).
void uiDrawPageRibbons(const App *app);
// The reading bar's bookmark button at (x, y): filled when the page shown is bookmarked.
void uiDrawMarkButton(const App *app, int x, int y);
// The contents panel's tabs, Contents and Bookmarks, from (x, y).
void uiDrawContentsTabs(const App *app, int x, int y);
// The open book's bookmarks, rows of CONTENTS_ROW_H from y (the shown ones from contents_first).
void uiDrawMarksList(const App *app, int x, int y, int w, int rows);
