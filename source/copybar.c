#include "copybar.h"

CopyBar copyBarOf(int64_t done, int64_t total, uint64_t now_ns)
{
    if (total > 0) {
        float share = done <= 0 ? 0.0f : done >= total ? 1.0f : (float)((double)done / (double)total);
        return (CopyBar){0.0f, share, true};
    }
    // There and back: 0 to 1 in the first trip, 1 to 0 in the second.
    uint64_t at = now_ns % (2 * COPYBAR_TRIP_NS);
    float t = (float)((double)(at < COPYBAR_TRIP_NS ? at : 2 * COPYBAR_TRIP_NS - at) / (double)COPYBAR_TRIP_NS);
    float from = t * (1.0f - COPYBAR_PIECE);
    return (CopyBar){from, from + COPYBAR_PIECE, false};
}
