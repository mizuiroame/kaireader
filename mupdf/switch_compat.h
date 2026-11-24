// SPDX-License-Identifier: MIT
// Included before every MuPDF source on Switch 2 (newlib).
#ifndef KAIREADER_SWITCH_COMPAT_H
#define KAIREADER_SWITCH_COMPAT_H

#include <sys/types.h>
#include <time.h>

// newlib's BSD names, which clash with MuPDF's fields (fz_stext_char.quad).
#undef quad
#undef physadr

// newlib has no timegm: seconds since 1970 for a broken-down UTC time.
static inline time_t kai_timegm(struct tm *tm)
{
    long long y = tm->tm_year + 1900LL + tm->tm_mon / 12;
    int m = tm->tm_mon % 12;
    if (m < 0) {
        m += 12;
        y--;
    }
    // Days from 1970-01-01 to the 1st of the month (Howard Hinnant's days_from_civil).
    long long yy = m < 2 ? y - 1 : y;
    long long era = (yy >= 0 ? yy : yy - 399) / 400;
    long long yoe = yy - era * 400;
    long long doy = (153LL * (m + (m > 1 ? -2 : 10)) + 2) / 5;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468 + (tm->tm_mday - 1);
    return (time_t)(days * 86400LL + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec);
}
#define timegm kai_timegm

#endif
