// Touch targets. The UI adds a region for each control as it draws it; a
// tap is matched against the regions of the last frame, topmost first, so
// touch areas always match what is on screen.
#pragma once

#include <stdbool.h>

#define HIT_MAX_REGIONS 256

typedef enum {
    HIT_BOOK,         // arg: a book's place in the library's order. A tap opens it.
    HIT_LIBRARY,      // The library's grid, for drags.
    HIT_EXIT,         // The X in the corner: quits.
    HIT_MENU_BUTTON,  // The title: opens the + menu.
    HIT_MENU_ITEM,    // arg: a row of the menu. A tap picks it (or steps it, for a value).
    HIT_MENU_STEP,    // arg: row * 2, + 1 for the right half: steps a value back or forward.
    HIT_OVERLAY_OUT,  // Outside a panel: closes it.
    HIT_NOTICE,       // A notice bubble: a tap sends it away.
    HIT_PAGE,         // The page: left third back, right third forward, the middle shows the bars.
    HIT_BACK,         // The reader's back arrow: to the library.
    HIT_SLIDER,       // The reader's page slider: a tap or drag goes there.
    HIT_CONTENTS,     // The reader's contents button.
    HIT_CHAPTER,      // arg: an entry of the contents. A tap goes there.
    HIT_RESCAN,       // Looks for books again.
} HitKind;

typedef struct {
    int x, y, w, h;
    HitKind kind;
    int arg;
} HitRegion;

void uiHitReset(void);
void uiHitAdd(int x, int y, int w, int h, HitKind kind, int arg);
// Targets added from now on are cut to between top and bottom (a list's place).
void uiHitClip(int top, int bottom);
// The topmost region at (x, y), or NULL.
const HitRegion *uiHitFind(int x, int y);
// The regions of the last frame, bottom first: for checks.
int uiHitCount(void);
const HitRegion *uiHitAt(int index);
