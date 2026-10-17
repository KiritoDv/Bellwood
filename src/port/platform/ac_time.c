#include "ac_port.h"

#include <time.h>

#define AC_MONTH_MAX 12
#define AC_WEEK_DAY_MAX 7
#define AC_YEAR_DAY_MAX 365
#define AC_SECS_IN_MIN 60
#define AC_SECS_IN_HOUR (AC_SECS_IN_MIN * 60)
#define AC_SECS_IN_DAY (AC_SECS_IN_HOUR * 24)
#define AC_SECS_IN_YEAR (AC_SECS_IN_DAY * 365)
#define AC_USEC_MAX 1000
#define AC_MSEC_MAX 1000

#define AC_BIAS (2000 * 365 + (2000 + 3) / 4 - (2000 - 1) / 100 + (2000 - 1) / 400)

static const int sYearDays[AC_MONTH_MAX] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static const int sLeapYearDays[AC_MONTH_MAX] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

static int IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int GetYearDays(int year, int mon) {
    const int* md = IsLeapYear(year) ? sLeapYearDays : sYearDays;

    return md[mon];
}

static int GetLeapDays(int year) {
    if (year < 1) {
        return 0;
    }
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

static void GetDates(int days, ACCalendarTime* td) {
    int year;
    int n;
    int month;
    const int* md;

    td->wday = (days + 6) % AC_WEEK_DAY_MAX;

    for (year = days / AC_YEAR_DAY_MAX; days < (n = year * AC_YEAR_DAY_MAX + GetLeapDays(year)); year--) {}

    days -= n;
    td->year = year;
    td->yday = days;

    md = IsLeapYear(year) ? sLeapYearDays : sYearDays;
    for (month = AC_MONTH_MAX; days < md[--month];) {}
    td->mon = month;
    td->mday = days - md[month] + 1;
}

void AC_TicksToCalendarTime(ACTime ticks, ACCalendarTime* td) {
    int days;
    int secs;
    ACTime d;

    d = ticks % ACSecondsToTicks(1);
    if (d < 0) {
        d += ACSecondsToTicks(1);
    }

    td->usec = (int)(ACTicksToMicroseconds(d) % AC_USEC_MAX);
    td->msec = (int)(ACTicksToMilliseconds(d) % AC_MSEC_MAX);

    ticks -= d;

    days = (int)(ACTicksToSeconds(ticks) / AC_SECS_IN_DAY) + AC_BIAS;
    secs = (int)(ACTicksToSeconds(ticks) % AC_SECS_IN_DAY);
    if (secs < 0) {
        days -= 1;
        secs += AC_SECS_IN_DAY;
    }

    GetDates(days, td);
    td->hour = secs / 60 / 60;
    td->min = secs / 60 % 60;
    td->sec = secs % 60;
}

ACTime AC_CalendarTimeToTicks(const ACCalendarTime* td) {
    ACTime secs;
    int ov_mon;
    int mon;
    int year;

    ov_mon = td->mon / AC_MONTH_MAX;
    mon = td->mon - (ov_mon * AC_MONTH_MAX);

    if (mon < 0) {
        mon += AC_MONTH_MAX;
        ov_mon--;
    }

    year = td->year + ov_mon;

    secs = (ACTime)AC_SECS_IN_YEAR * year +
           (ACTime)AC_SECS_IN_DAY * (GetLeapDays(year) + GetYearDays(year, mon) + td->mday - 1) +
           (ACTime)AC_SECS_IN_HOUR * td->hour + (ACTime)AC_SECS_IN_MIN * td->min + td->sec - (ACTime)0xEB1E1BF80LL;

    return ACSecondsToTicks(secs) + ACMillisecondsToTicks((ACTime)td->msec) + ACMicrosecondsToTicks((ACTime)td->usec);
}

ACTime AC_GetTime(void) {
    struct timespec ts;
    struct tm lt;
    ACCalendarTime td;
    time_t t;

#if defined(_WIN32)
    timespec_get(&ts, TIME_UTC);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    t = ts.tv_sec;
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif

    td.sec = lt.tm_sec > 59 ? 59 : lt.tm_sec;
    td.min = lt.tm_min;
    td.hour = lt.tm_hour;
    td.mday = lt.tm_mday;
    td.mon = lt.tm_mon;
    td.year = lt.tm_year + 1900;
    td.wday = lt.tm_wday;
    td.yday = lt.tm_yday;
    td.msec = (int)(ts.tv_nsec / 1000000);
    td.usec = (int)((ts.tv_nsec / 1000) % 1000);

    return AC_CalendarTimeToTicks(&td);
}

ACTick AC_GetTick(void) {
    return (ACTick)AC_GetTime();
}
