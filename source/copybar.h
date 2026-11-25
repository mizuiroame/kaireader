// The part of a copy's bar that's filled. With the size known it fills up to
// the share copied; without it, a piece of the bar slides to and fro, so it's
// seen to be working without claiming how far along it is.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define COPYBAR_PIECE 0.25f             // The sliding piece: this much of the bar.
#define COPYBAR_TRIP_NS 1200000000ull  // One way across, in nanoseconds.

typedef struct {
    float from, to;  // 0 (the bar's left end) to 1 (its right end).
    bool known;      // The size is known: from is 0 and to the share copied.
} CopyBar;

CopyBar copyBarOf(int64_t done, int64_t total, uint64_t now_ns);
