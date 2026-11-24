// Zoom on fixed pages (PDF, comics): up and down or two fingers bring the page
// closer, the left stick or a finger moves it. What's shown is scaled at once;
// once the zoom rests a moment the page is rendered again at it, sharp
// (ReadingLayout.zoom). Two pages side by side zoom together, as one spread.
#include <math.h>

#include "app.h"
#include "gfx.h"
#include "lang.h"

#define ZOOM_MAX 3.0f
#define ZOOM_STEP 1.25f
#define ZOOM_REST_NS 300000000ull  // The zoom unchanged this long: render it sharp.
#define PAN_STEP 48.0f             // Layout pixels a stick push moves the zoomed page.

bool appZoomable(const App *app)
{
    return app->view == VIEW_READER && app->ready && !app->reflowable;
}

bool appZoomed(const App *app)
{
    return app->zoom > 1.001f;
}

// What's shown at zoom 1 (the page, or both of the spread): its box, layout pixels. False if none is here.
static bool fitBox(const App *app, float *x0, float *y0, float *x1, float *y1)
{
    bool any = false;
    for (int side = 0; side < (appTwoPages(app) ? 2 : 1); side++) {
        int x, y, w, h;
        if (!appFitRect(app, app->page + side, &x, &y, &w, &h))
            continue;
        *x0 = any && *x0 < x ? *x0 : x;
        *y0 = any && *y0 < y ? *y0 : y;
        *x1 = any && *x1 > x + w ? *x1 : x + w;
        *y1 = any && *y1 > y + h ? *y1 : y + h;
        any = true;
    }
    return any;
}

// One axis: the pan that keeps [a, b] (at zoom 1) over the screen's [0, size] when zoomed:
// no gap at an edge where it's bigger, centered where it's smaller.
static float clampAxis(float pan, float a, float b, float size, float zoom)
{
    float mid = size / 2, lo = mid + (a - mid) * zoom, hi = mid + (b - mid) * zoom;  // Its ends with no pan.
    if (hi - lo <= size)
        return mid - (lo + hi) / 2;
    float most = -lo, least = size - hi;  // pan <= -lo keeps its start at 0 or before; >= size - hi, its end.
    return pan > most ? most : pan < least ? least : pan;
}

// Keeps the zoomed page (or spread) over the screen.
static void clampPan(App *app)
{
    float x0, y0, x1, y1;
    if (!fitBox(app, &x0, &y0, &x1, &y1)) {
        app->pan_x = app->pan_y = 0;
        return;
    }
    app->pan_x = clampAxis(app->pan_x, x0, x1, (float)gfxLayoutWidth(), app->zoom);
    app->pan_y = clampAxis(app->pan_y, y0, y1, (float)gfxLayoutHeight(), app->zoom);
}

void appZoomTo(App *app, float zoom, float cx, float cy, uint64_t now_ns)
{
    if (!appZoomable(app))
        return;
    zoom = zoom < 1.0f ? 1.0f : zoom > ZOOM_MAX ? ZOOM_MAX : zoom;
    // The page's point under (cx, cy) stays there. Shown at screen middle + (fitted - middle) * zoom + pan.
    float mx = gfxLayoutWidth() / 2.0f, my = gfxLayoutHeight() / 2.0f;
    float px = (cx - mx - app->pan_x) / app->zoom, py = (cy - my - app->pan_y) / app->zoom;  // From the middle, fitted.
    app->pan_x = cx - mx - px * zoom;
    app->pan_y = cy - my - py * zoom;
    app->zoom = zoom;
    app->zoom_changed_ns = now_ns;
    clampPan(app);
}

void appZoomStep(App *app, int dir, uint64_t now_ns)
{
    if (!appZoomable(app))
        return;
    appZoomTo(app, app->zoom * (dir > 0 ? ZOOM_STEP : 1.0f / ZOOM_STEP), gfxLayoutWidth() / 2.0f,
              gfxLayoutHeight() / 2.0f, now_ns);
}

void appZoomPan(App *app, float dx, float dy)
{
    if (!appZoomed(app))
        return;
    app->pan_x += dx;
    app->pan_y += dy;
    clampPan(app);
}

void appZoomPanStep(App *app, int dx, int dy)
{
    appZoomPan(app, -dx * PAN_STEP, -dy * PAN_STEP);  // The stick looks that way: the page moves the other.
}

void appZoomToStart(App *app)
{
    if (!appZoomed(app)) {
        app->pan_x = app->pan_y = 0;
        return;
    }
    app->pan_x = app->pan_y = 1e6f;  // The top left of the next page (or spread): as it's read.
    clampPan(app);
}

void appZoomReset(App *app)
{
    app->zoom = app->render_zoom = 1.0f;
    app->pan_x = app->pan_y = 0;
    app->pinching = app->touch_held = false;
}

void appZoomTick(App *app, uint64_t now_ns)
{
    if (!appZoomable(app) || fabsf(app->zoom - app->render_zoom) < 0.01f || now_ns - app->zoom_changed_ns < ZOOM_REST_NS)
        return;
    app->render_zoom = app->zoom;
    appRenderAgain(app);  // Sharp at the new zoom; the scaled pages show until they come.
}

bool appZoomTouches(App *app, int count, float x0, float y0, float x1, float y1, uint64_t now_ns)
{
    if (count == 0) {
        app->pinching = app->touch_held = false;
        return false;
    }
    if (count < 2) {
        app->pinching = false;
        return app->touch_held;  // The finger left of two: nothing until it's up too.
    }
    float distance = hypotf(x1 - x0, y1 - y0), mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
    if (!app->pinching) {
        if (!appZoomable(app) || app->panel != PANEL_NONE)
            return app->touch_held;
        app->pinching = app->touch_held = true;
        app->pinch_distance = distance;
        app->pinch_zoom = app->zoom;
        app->pinch_x = mx;
        app->pinch_y = my;
        return true;
    }
    if (app->pinch_distance > 0 && distance > 0)
        appZoomTo(app, app->pinch_zoom * distance / app->pinch_distance, mx, my, now_ns);
    appZoomPan(app, mx - app->pinch_x, my - app->pinch_y);  // Both fingers moving: the page too.
    app->pinch_x = mx;
    app->pinch_y = my;
    return true;
}
