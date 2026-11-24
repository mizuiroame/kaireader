// Turning the screen off while music plays, to save battery. In handheld
// mode it uses the backlight service when the console allows it; docked, or
// without it, the app only draws a black screen.
#pragma once

#include <stdbool.h>

void screenInit(void);
void screenExit(void);  // Turns the backlight back on.

void screenSetOn(bool on);
bool screenIsOn(void);
// True if the backlight itself can be turned off (not just a black screen).
bool screenHasBacklightControl(void);
