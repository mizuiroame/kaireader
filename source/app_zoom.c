// Zoom on fixed pages (PDF, comics): up and down or two fingers bring the page
// closer, the left stick or a finger moves it. What's shown is scaled at once;
// once the zoom rests a moment the page is rendered again at it, sharp
// (ReadingLayout.zoom). Two pages side by side zoom together, as one spread.
#include <math.h>
#include <string.h>

#include "app.h"
#include "gfx.h"
#include "lang.h"

#define ZOOM_MAX 3.0f
#define ZOOM_STEP 1.25f
#define ZOOM_REST_NS 300000000ull  // The zoom unchanged this long: render it sharp.
#define PAN_STEP 48.0f             // Layout pixels a stick push moves the zoomed page.
#define PART_MARGIN 96             // Pixels around the part on screen rendered too: a little move stays sharp.

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
    app->zoom_changed_ns = app->view_moved_ns = now_ns;
    clampPan(app);
}

void appZoomStep(App *app, int dir, uint64_t now_ns)
{
    if (!appZoomable(app))
        return;
    appZoomTo(app, app->zoom * (dir > 0 ? ZOOM_STEP : 1.0f / ZOOM_STEP), gfxLayoutWidth() / 2.0f,
              gfxLayoutHeight() / 2.0f, now_ns);
}

void appZoomPan(App *app, float dx, float dy, uint64_t now_ns)
{
    if (!appZoomed(app))
        return;
    app->pan_x += dx;
    app->pan_y += dy;
    clampPan(app);
    app->view_moved_ns = now_ns;
}

void appZoomPanStep(App *app, int dx, int dy, uint64_t now_ns)
{
    appZoomPan(app, -dx * PAN_STEP, -dy * PAN_STEP, now_ns);  // The stick looks that way: the page moves the other.
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
    appForgetParts(app);
}

void appZoomTick(App *app, uint64_t now_ns)
{
    if (!appZoomable(app) || fabsf(app->zoom - app->render_zoom) < 0.01f || now_ns - app->zoom_changed_ns < ZOOM_REST_NS)
        return;
    app->render_zoom = app->zoom;
    appRenderAgain(app);  // Sharp at the new zoom; the scaled pages show until they come.
}

// The part of page on screen, in its pixels at the zoom shown (the whole page's full_w x full_h),
// margin pixels around; false if none of it shows.
static bool shownPart(const App *app, int page, int margin, ReadingPartAsk *out, int *full_w, int *full_h)
{
    float iw, ih, made;
    int x, y, w, h;
    if (!appPageImageZoom(app, page, &iw, &ih, &made))
        return false;
    appPageRect(app, page, &x, &y, &w, &h);
    *full_w = (int)(iw / made * app->zoom + 0.5f);
    *full_h = (int)(ih / made * app->zoom + 0.5f);
    float x0 = x > 0 ? x : 0, y0 = y > 0 ? y : 0;
    float x1 = x + w < gfxLayoutWidth() ? x + w : gfxLayoutWidth(), y1 = y + h < gfxLayoutHeight() ? y + h : gfxLayoutHeight();
    if (x1 <= x0 || y1 <= y0 || w <= 0 || h <= 0)
        return false;
    float sx = (float)*full_w / w, sy = (float)*full_h / h;
    int px0 = (int)((x0 - x) * sx) - margin, py0 = (int)((y0 - y) * sy) - margin;
    int px1 = (int)((x1 - x) * sx + 0.5f) + margin, py1 = (int)((y1 - y) * sy + 0.5f) + margin;
    px0 = px0 < 0 ? 0 : px0;
    py0 = py0 < 0 ? 0 : py0;
    px1 = px1 > *full_w ? *full_w : px1;
    py1 = py1 > *full_h ? *full_h : py1;
    px1 = px1 - px0 > DOC_RENDER_MAX ? px0 + DOC_RENDER_MAX : px1;
    py1 = py1 - py0 > DOC_RENDER_MAX ? py0 + DOC_RENDER_MAX : py1;
    *out = (ReadingPartAsk){page, app->zoom, px0, py0, px1 - px0, py1 - py0};
    return px1 > px0 && py1 > py0;
}

// True when have (of the same page and zoom) holds all of need.
static bool covers(const ReadingPartAsk *have, const ReadingPartAsk *need)
{
    return have->page == need->page && fabsf(have->zoom - need->zoom) < 0.01f && have->x <= need->x &&
           have->y <= need->y && have->x + have->w >= need->x + need->w && have->y + have->h >= need->y + need->h;
}

void appAskParts(App *app, uint64_t now_ns)
{
    if (app->parts_generation != app->generation) {  // Asks of a layout gone won't come.
        app->parts_generation = app->generation;
        app->parts_asked_count = 0;
    }
    if (!appZoomable(app) || !appZoomed(app) || fabsf(app->zoom - app->render_zoom) >= 0.01f ||
        now_ns - app->view_moved_ns < ZOOM_REST_NS)
        return;
    ReadingPartAsk asks[READING_PARTS_MAX];
    int n = 0;
    for (int side = 0; side < (appTwoPages(app) ? 2 : 1); side++) {
        int page = app->page + side, full_w, full_h;
        float iw, ih, made;
        ReadingPartAsk need, want;
        // The page itself first, at this zoom; a part only where it came smaller (too big whole).
        if (page >= app->pages || !appPageCurrent(app, page) || !appPageImageZoom(app, page, &iw, &ih, &made) ||
            made >= app->zoom - 0.01f || !shownPart(app, page, 0, &need, &full_w, &full_h))
            continue;
        bool have = false;
        for (int i = 0; i < READING_PARTS_MAX && !have; i++) {
            const PagePart *p = &app->parts[i];
            ReadingPartAsk kept = {p->page, p->zoom, p->x, p->y, p->image.width, p->image.height};
            have = p->page >= 0 && p->image.pixels && covers(&kept, &need);
        }
        for (int i = 0; i < app->parts_asked_count && !have; i++)
            have = covers(&app->parts_asked[i], &need);
        if (!have && shownPart(app, page, PART_MARGIN, &want, &full_w, &full_h))
            asks[n++] = want;
    }
    if (n == 0)
        return;
    memcpy(app->parts_asked, asks, (size_t)n * sizeof(*asks));
    app->parts_asked_count = n;
    readingWantParts(app->generation, asks, n);
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
    appZoomPan(app, mx - app->pinch_x, my - app->pinch_y, now_ns);  // Both fingers moving: the page too.
    app->pinch_x = mx;
    app->pinch_y = my;
    return true;
}
