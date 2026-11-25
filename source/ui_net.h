// The library's source pill (SD card or network folder), and what only the
// network folder's library shows: its looking-through, why it's empty, and a
// book being copied to the card to open.
#pragma once

#include <stdint.h>

#include "app.h"
#include "gfx.h"

// Where the library comes from, after the title, once there's a network folder:
// "[X] ● SD card" or "[X] ● Network folder", as KaiPlayer's. A tap switches it.
// Its right end (where the title ends without it).
int uiDrawSourcePill(const App *app);
// After the library's title and count (x, cy its line's middle): the folder being looked through.
void uiDrawNetScan(int tab, int x, float cy, uint64_t now_ns);
// A network folder's or book server's library with no books: looking, why it couldn't be read, or none there.
void uiDrawNetEmpty(const App *app, uint64_t now_ns);
// "Loading covers" with a spinner, its right end at right (the source's color for the network's).
// Only the spinner if the words would reach left (the source pill's end).
void uiDrawCoversPill(int left, int right, uint64_t now_ns, int tab);
// A source's name ("SD card", "Network folder", "Book server"), translated, and its color.
const char *uiSourceName(int tab);
Color uiSourceColor(int tab);
// A book being copied to the card, over the library.
void uiDrawNetCopy(const App *app, uint64_t now_ns);
