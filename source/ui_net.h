// The library's source pill (SD card or network folder), and what only the
// network folder's library shows: its looking-through, why it's empty, and a
// book being copied to the card to open.
#pragma once

#include <stdint.h>

#include "app.h"

// Where the library comes from, after the title, once there's a network folder:
// "[X] ● SD card" or "[X] ● Network folder", as KaiPlayer's. A tap switches it.
void uiDrawSourcePill(const App *app);
// After the library's title and count (x, cy its line's middle): the folder being looked through.
void uiDrawNetScan(int x, float cy, uint64_t now_ns);
// The network folder's library with no books: looking, why it couldn't be read, or none there.
void uiDrawNetEmpty(const App *app, uint64_t now_ns);
// A book being copied to the card, over the library.
void uiDrawNetCopy(const App *app);
