#include <assert.h>
#include "calendar.h"

int main(void)
{
    calendar_date_t date;

    assert(calendar_get(2024, 2, 10, &date));
    assert(date.year == 2024 && date.month == 1 && date.day == 1 && !date.is_leap_month);
    assert(calendar_get(2025, 1, 29, &date));
    assert(date.year == 2025 && date.month == 1 && date.day == 1 && !date.is_leap_month);
    assert(calendar_get(2025, 11, 16, &date));
    assert(date.year == 2025 && date.month == 9 && date.day == 27 && date.solar_term == CALENDAR_NO_SOLAR_TERM);
    assert(calendar_get(2026, 9, 23, &date));
    assert(date.year == 2026 && date.month == 8 && date.day == 13 && date.solar_term == 17);
    assert(calendar_get(2026, 9, 18, &date));
    assert(date.solar_term == CALENDAR_NO_SOLAR_TERM);
    return 0;
}
