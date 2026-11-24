#include "motion.h"

#include <stdio.h>
#include <string.h>

#ifdef __SWITCH__
#include <switch.h>

#define HANDLES 4  // Handheld, the two Joy-Con held together, a Pro Controller.

static HidSixAxisSensorHandle g_handles[HANDLES];
static bool g_started[HANDLES];
static Result g_get[HANDLES], g_start[HANDLES];
static bool g_init;

void motionInit(void)
{
    if (g_init)
        return;
    g_init = true;
    memset(g_started, 0, sizeof(g_started));
    g_get[0] = hidGetSixAxisSensorHandles(&g_handles[0], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    g_get[1] = hidGetSixAxisSensorHandles(&g_handles[1], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    g_get[2] = g_get[1];
    g_get[3] = hidGetSixAxisSensorHandles(&g_handles[3], 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey);
    for (int i = 0; i < HANDLES; i++) {
        g_start[i] = R_SUCCEEDED(g_get[i]) ? hidStartSixAxisSensor(g_handles[i]) : g_get[i];
        g_started[i] = R_SUCCEEDED(g_get[i]) && R_SUCCEEDED(g_start[i]);
    }
}

void motionExit(void)
{
    for (int i = 0; i < HANDLES; i++)
        if (g_started[i])
            hidStopSixAxisSensor(g_handles[i]);
    memset(g_started, 0, sizeof(g_started));
    g_init = false;
}

bool motionGravity(float *x, float *y, float *z, int *source)
{
    for (int i = 0; i < HANDLES; i++) {
        if (!g_started[i])
            continue;
        HidSixAxisSensorState state;
        if (hidGetSixAxisSensorStates(g_handles[i], &state, 1) < 1)
            continue;
        float ax = state.acceleration.x, ay = state.acceleration.y, az = state.acceleration.z;
        if (ax == 0 && ay == 0 && az == 0)
            continue;  // Started, but nothing comes from it (not connected).
        *x = ax;
        *y = ay;
        *z = az;
        *source = i;
        return true;
    }
    return false;
}

void motionDescribe(char *out, size_t size)
{
    static const char *const NAMES[HANDLES] = {"handheld", "joy-con left", "joy-con right", "pro"};
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < HANDLES && used < size; i++) {
        int n = snprintf(out + used, size - used, "%s: get 0x%x start 0x%x%s\n", NAMES[i], (unsigned)g_get[i],
                         (unsigned)g_start[i], g_started[i] ? " (started)" : "");
        if (n < 0)
            break;
        used += (size_t)n;
    }
}

#else  // The host builds: no sensors.

void motionInit(void) {}
void motionExit(void) {}
bool motionGravity(float *x, float *y, float *z, int *source)
{
    (void)x, (void)y, (void)z, (void)source;
    return false;
}
void motionDescribe(char *out, size_t size)
{
    if (size)
        snprintf(out, size, "no sensors here\n");
}

#endif
