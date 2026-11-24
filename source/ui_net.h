// The library's source pill (SD card or network folder) and the network folder's view.
#pragma once

#include <stdint.h>

#include "app.h"

// Where the library comes from, after the title, once there's a network folder:
// "[X] ● SD card" or "[X] ● Network folder", as KaiPlayer's. A tap switches it.
void uiDrawSourcePill(const App *app);
// The Network tab: the folder's folders and books, or why there are none, and a copy going on.
void uiDrawNetwork(App *app, uint64_t now_ns);
