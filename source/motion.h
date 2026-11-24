// The console's motion sensors (accelerometer), for turning the screen with
// the console: which way gravity pulls. Handheld first, then Joy-Con held
// together, then a Pro Controller. Console only; the host builds have none.
#pragma once

#include <stdbool.h>
#include <stddef.h>

// Starts the sensors it can. Safe to call once; motionExit stops them.
void motionInit(void);
void motionExit(void);
// Gravity now, in g (x right, y up, z out of the screen, as the device reports
// it), and which controller gave it (0 handheld, 1-2 Joy-Con, 3 Pro). False if none reports.
bool motionGravity(float *x, float *y, float *z, int *source);
// What starting them returned, for a log: one line per controller tried.
void motionDescribe(char *out, size_t size);
