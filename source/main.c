// KaiReader: a book reader for Switch 2 homebrew (kaiHEN + libounce). Books
// from the SD card are opened and drawn with MuPDF (doc.h) on a thread of
// their own (reading.h); the screens are drawn with the GPU.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <switch.h>

#include "app.h"
#include "doc.h"
#include "approot.h"
#include "buttons.h"
#include "font.h"
#include "gfx.h"
#include "input.h"
#include "reading.h"
#include "sysinfo.h"
#include "ui.h"

#define SCREEN_W 1280
#define SCREEN_H 720
#define PANEL_W 1920  // In case the touch panel reports its own resolution.

static uint64_t nowNs(void)
{
    return armTicksToNs(armGetSystemTick());
}

static void readTouch(App *app, uint64_t now)
{
    static bool full_hd;  // Seen coordinates beyond 1280x720: scale them down.
    HidTouchScreenState state = {0};
    bool touching = hidGetTouchScreenStates(&state, 1) > 0 && state.count > 0;
    int x = 0, y = 0;
    if (touching) {
        x = (int)state.touches[0].x;
        y = (int)state.touches[0].y;
        if (x >= SCREEN_W || y >= SCREEN_H)
            full_hd = true;
        if (full_hd) {
            x = x * SCREEN_W / PANEL_W;
            y = y * SCREEN_W / PANEL_W;
        }
    }
    appHandleTouch(app, touching, x, y, now);
}

static void runLoop(App *app)
{
    PadState pad;
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    InputRepeat repeat;
    inputRepeatInit(&repeat, BUTTONS_REPEATING);
    while (!app->quit && appletMainLoop()) {
        padUpdate(&pad);
        uint64_t now = nowNs();
        uint64_t pressed = inputRepeatUpdate(&repeat, padGetButtonsDown(&pad), padGetButtons(&pad), now);
        appHandleButtons(app, pressed, now);
        readTouch(app, now);
        appUpdate(app, now);
        sysinfoGet(&app->sys, now);
        gfxBegin();
        uiDraw(app, now);
        gfxEnd();
    }
}

static char g_folder[APPROOT_PATH_MAX];

// The console's Japanese, Chinese and Korean fonts, for books that don't bring
// their own. They stay mapped while the font service is open (fontInit to fontExit).
static void giveBooksFonts(void)
{
    static const PlSharedFontType TYPES[DOC_FONT_COUNT] = {
        [DOC_FONT_JAPANESE] = PlSharedFontType_Standard,
        [DOC_FONT_CHINESE_SIMPLIFIED] = PlSharedFontType_ChineseSimplified,
        [DOC_FONT_CHINESE_TRADITIONAL] = PlSharedFontType_ChineseTraditional,
        [DOC_FONT_KOREAN] = PlSharedFontType_KO,
    };
    DocFont fonts[DOC_FONT_COUNT] = {0};
    if (fontFallbackCount() > 0) {
        for (int i = 0; i < DOC_FONT_COUNT; i++) {
            PlFontData font;
            if (R_SUCCEEDED(plGetSharedFontByType(&font, TYPES[i])) && font.address && font.size > 0)
                fonts[i] = (DocFont){font.address, font.size};
        }
    }
    docSetFallbackFonts(fonts);
}

// Shows why KaiReader can't start on libnx's text console until + is
// pressed, and writes it to error.txt in KaiReader's folder.
static void startupError(const char *message)
{
    char path[APPROOT_PATH_MAX + 16];
    snprintf(path, sizeof(path), "%s/error.txt", g_folder);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", message);
        fclose(f);
    }
    consoleInit(NULL);
    printf("\n  %s\n\n  Press + to exit.\n", message);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
}

int main(int argc, char **argv)
{
    appRootFolder(argc > 0 ? argv[0] : NULL, "kaireader", g_folder, sizeof(g_folder));
    appRootMakeFolder(g_folder);
    srand((unsigned)time(NULL));
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    if (!gfxInit()) {
        startupError("KaiReader could not start drawing (the GPU or its window).");
        return 1;
    }
    if (!fontInit()) {
        gfxExit();
        startupError("KaiReader could not load its fonts.");
        return 1;
    }
    giveBooksFonts();
    if (!readingStart()) {
        fontExit();
        gfxExit();
        startupError("KaiReader could not start its reading thread.");
        return 1;
    }
    sysinfoInit();
    static App app;
    appInit(&app, g_folder);

    runLoop(&app);

    appFree(&app);  // Where the book was left, and the settings.
    readingStop();
    sysinfoExit();
    fontExit();
    gfxExit();
    return 0;
}
