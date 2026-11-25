// A copy's bar: the share copied when the size is known, a sliding piece when it isn't.
#include "check.h"
#include "copybar.h"

#define TRIP COPYBAR_TRIP_NS

static void testKnownSize(void)
{
    CopyBar b = copyBarOf(0, 1000, 5);
    CHECK(b.known && b.from == 0.0f && b.to == 0.0f);
    b = copyBarOf(400, 1000, 5);
    CHECK(b.known && b.to > 0.399f && b.to < 0.401f);
    b = copyBarOf(1000, 1000, 5);
    CHECK(b.known && b.to == 1.0f);
    b = copyBarOf(1500, 1000, 5);  // A server that sends more than it said: full, not past the end.
    CHECK(b.known && b.to == 1.0f);
    b = copyBarOf(5000000000ll, 6000000000ll, 5);  // Over 4 GB.
    CHECK(b.known && b.to > 0.83f && b.to < 0.84f);
}

static void testUnknownSize(void)
{
    // Nothing said (-1) or 0: it moves with time, whatever was copied.
    CopyBar a = copyBarOf(123456, -1, 0), b = copyBarOf(123456, -1, TRIP / 2), c = copyBarOf(0, 0, TRIP);
    CHECK(!a.known && !b.known && !c.known);
    CHECK(a.from == 0.0f && a.to > 0.249f && a.to < 0.251f);  // Starts at the left end.
    CHECK(b.from > a.from && b.from < c.from);                 // Halfway across.
    CHECK(c.from > 0.749f && c.to > 0.999f && c.to <= 1.0f);    // At the right end after one trip.
    CopyBar back = copyBarOf(0, -1, TRIP + TRIP / 2);  // And back again.
    CHECK(back.from > b.from - 0.001f && back.from < b.from + 0.001f);
    CopyBar again = copyBarOf(0, -1, 2 * TRIP);
    CHECK(again.from == 0.0f);
    for (uint64_t t = 0; t < 3 * TRIP; t += TRIP / 7) {  // Always inside the bar, always the same length.
        CopyBar p = copyBarOf(0, -1, t);
        CHECK(p.from >= 0.0f && p.to <= 1.0001f && p.to - p.from > 0.249f && p.to - p.from < 0.251f);
    }
}

void testCopyBar(void)
{
    testKnownSize();
    testUnknownSize();
}
