// Scrolling as on a phone: the glide after a flick.
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "kinetic.h"

#define MS 1000000ull
#define FRAME (16 * MS)

// A drag of d pixels a frame for frames frames, from t; returns the time after.
static uint64_t drag(Kinetic *k, int d, int frames, uint64_t t)
{
    kineticDrag(k, 0, t);
    for (int i = 0; i < frames; i++) {
        t += FRAME;
        kineticDrag(k, d, t);
    }
    return t;
}

// The glide's whole way, and how many frames it took.
static int glide(Kinetic *k, uint64_t t, int *frames)
{
    int total = 0;
    *frames = 0;
    while (kineticGliding(k) && *frames < 10000) {
        t += FRAME;
        total += kineticStep(k, t);
        (*frames)++;
    }
    return total;
}

static void testGlide(void)
{
    Kinetic k;
    memset(&k, 0, sizeof(k));
    uint64_t t = drag(&k, 30, 6, 1000 * MS);  // A flick up: 30 pixels a frame.
    kineticRelease(&k, t);
    CHECK(kineticGliding(&k) && k.velocity > 1500 && k.velocity < 2000);
    int frames, way = glide(&k, t, &frames);
    CHECK(way > 300 && way < 1200);           // It goes on, then slows down and stops.
    CHECK(frames > 30 && frames < 300 && !kineticGliding(&k));

    t = drag(&k, -20, 6, 5000 * MS);          // Down: the other way.
    kineticRelease(&k, t);
    CHECK(glide(&k, t, &frames) < 0);

    t = drag(&k, 30, 6, 9000 * MS);           // Held still before letting go: no glide.
    kineticRelease(&k, t + 200 * MS);
    CHECK(!kineticGliding(&k));

    t = drag(&k, 1, 6, 12000 * MS);           // A slow drag: no glide either.
    kineticRelease(&k, t);
    CHECK(!kineticGliding(&k));

    t = drag(&k, 30, 6, 15000 * MS);          // A press stops it where it is.
    kineticRelease(&k, t);
    kineticStep(&k, t + FRAME);
    kineticStop(&k);
    CHECK(!kineticGliding(&k) && kineticStep(&k, t + 2 * FRAME) == 0);

    t = drag(&k, 4000, 3, 20000 * MS);        // A wild flick has a top speed.
    kineticRelease(&k, t);
    CHECK(k.velocity <= KINETIC_MAX_SPEED);
}

void testKinetic(void)
{
    testGlide();
}
