// Mizuiro's mark for the About page: the 水色 seal and the MIZUIRO wordmark,
// pictures made by switch2homebrew-submit/tools/brand.py (data/brand_*.png).
#pragma once

// Draws the seal seal_h layout pixels tall at (x, y), "Made by" and the
// wordmark beside it. Returns the mark's height (0 if its pictures can't be read).
int uiDrawBrand(int x, int y, int seal_h);
// KaiReader's icon (data/about_icon.png: without the seal, rounded), size layout pixels square.
void uiDrawAppIcon(int x, int y, int size);
// The same with its right end at right.
int uiDrawBrandRight(int right, int y, int seal_h);
