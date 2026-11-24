// touch.c: taps, drags and a long touch.
#include "check.h"
#include "touch.h"

#define MS 1000000ull

void testTouch(void)
{
    TouchTracker t;
    touchReset(&t);
    CHECK(touchUpdate(&t, true, 100, 100, 0).type == TOUCH_PRESS);
    CHECK(touchUpdate(&t, true, 102, 101, 100 * MS).type == TOUCH_NONE);
    CHECK(touchUpdate(&t, false, 0, 0, 200 * MS).type == TOUCH_TAP);

    // Held still: a long touch once, and no tap when it's lifted.
    CHECK(touchUpdate(&t, true, 50, 60, 1000 * MS).type == TOUCH_PRESS);
    CHECK(touchUpdate(&t, true, 51, 60, 1000 * MS + TOUCH_LONG_NS - MS).type == TOUCH_NONE);
    TouchEvent e = touchUpdate(&t, true, 51, 60, 1000 * MS + TOUCH_LONG_NS);
    CHECK(e.type == TOUCH_LONG && e.x == 50 && e.y == 60);
    CHECK(touchUpdate(&t, true, 51, 60, 1000 * MS + TOUCH_LONG_NS + 100 * MS).type == TOUCH_NONE);
    CHECK(touchUpdate(&t, false, 0, 0, 1000 * MS + TOUCH_LONG_NS + 200 * MS).type == TOUCH_NONE);

    // Moved first: a drag, never a long touch.
    touchUpdate(&t, true, 10, 10, 5000 * MS);
    CHECK(touchUpdate(&t, true, 10, 80, 5100 * MS).type == TOUCH_DRAG);
    CHECK(touchUpdate(&t, true, 10, 80, 5000 * MS + TOUCH_LONG_NS + MS).type == TOUCH_DRAG);
    CHECK(touchUpdate(&t, false, 0, 0, 6000 * MS).type == TOUCH_DRAG_END);
}
