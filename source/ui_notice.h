// The notice bubbles (notice.h): at the top, centered, over everything, newest on top.
#pragma once

#include "notice.h"

// Draws the bubbles noticeVisible gave, and their tap targets (HIT_NOTICE, the notice's id).
void uiDrawNotices(const NoticeView *views, int count);
