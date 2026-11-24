// The library's tabs (the card's books, the network folder) and the Network tab.
#pragma once

#include <stdint.h>

#include "app.h"

// The tabs, centered in the header between the ZL and ZR hints (as KaiPlayer's).
void uiDrawLibTabs(const App *app);
// The Network tab: the folder's folders and books, or why there are none, and a copy going on.
void uiDrawNetwork(App *app, uint64_t now_ns);
