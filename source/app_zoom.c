// Zoom on fixed pages (PDF, comics): up and down or two fingers bring the page
// closer, the left stick or a finger moves it. What's shown is scaled at once;
// once the zoom rests a moment the page is rendered again at it, sharp
// (ReadingLayout.zoom). One page at a time: with two side by side it's off.
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
    return app->view == VIEW_READER && app->ready && !app->reflowable && !appTwoPages(app);
}

bool appZoomed(const App *app)
{
    return app->zoom > 1.001f;
}

// The page's size on screen at zoom 1 (fitted), layout pixels; false if it isn't here.
static bool fitSize(const App *app, float *w, float *h)
{
    float iw, ih, at;
    if (!appPageImageZoom(app, app->page, &iw, &ih, &at))
        return false;
    float per = GFX_SCALE * gfxDetail() * at;
    *w = iw / per;
    *h = ih / per;
    return true;
}

// Keeps the zoomed page over the screen: no gap at an edge where it's bigger than the screen.
static void clampPan(App *app)
{
    float w, h;
    if (!fitSize(app, &w, &h)) {
        app->pan_x = app->pan_y = 0;
        return;
    }
    float over_x = (w * app->zoom - gfxLayoutWidth()) / 2, over_y = (h * app->zoom - gfxLayoutHeight()) / 2;
    over_x = over_x < 0 ? 0 : over_x;
    over_y = over_y < 0 ? 0 : over_y;
    app->pan_x = app->pan_x < -over_x ? -over_x : app->pan_x > over_x ? over_x : app->pan_x;
    app->pan_y = app->pan_y < -over_y ? -over_y : app->pan_y > over_y ? over_y : app->pan_y;
}

void appZoomTo(App *app, float zoom, float cx, float cy, uint64_t now_ns)
{
    if (!appZoomable(app))
        return;
    zoom = zoom < 1.0f ? 1.0f : zoom > ZOOM_MAX ? ZOOM_MAX : zoom;
    // The page's point under (cx, cy) stays there: pan is the page middle's offset from the screen's.
    float mx = gfxLayoutWidth() / 2.0f + app->pan_x, my = gfxLayoutHeight() / 2.0f + app->pan_y;
    float k = zoom / app->zoom;
    app->pan_x = cx + (mx - cx) * k - gfxLayoutWidth() / 2.0f;
    app->pan_y = cy + (my - cy) * k - gfxLayoutHeight() / 2.0f;
    app->zoom = zoom;
    app->zoom_changed_ns = now_ns;
    clampPan(app);
}

void appZoomStep(App *app, int dir, uint64_t now_ns)
{
    if (!appZoomable(app)) {
        if (app->view == VIEW_READER && appTwoPages(app))
            appNotify(app, NOTICE_INFO, T("Zoom shows one page: set Pages to One in Settings."), now_ns);
        return;
    }
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
    app->pan_x = app->pan_y = 1e6f;  // The top left of the next page: as it's read.
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
