#include "calendar.h"
#include "calendar_data.h"

static int32_t solar_days(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint16_t before_month[] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint16_t previous_year = year - 1;
    int32_t days = previous_year * 365L + previous_year / 4 - previous_year / 100 + previous_year / 400;

    days += before_month[month] + day;
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        days++;
    return days;
}

static uint16_t bits(uint32_t value, uint8_t length, uint8_t shift)
{
    return (value >> shift) & ((1U << length) - 1);
}

uint8_t calendar_get(uint16_t year, uint8_t month, uint8_t day, calendar_date_t *date)
{
    uint32_t packed_date;
    uint32_t months;
    int16_t offset;
    int16_t index;
    uint8_t lunar_month;
    uint8_t term;
    uint8_t i;

    if (!date || year < CALENDAR_MIN_YEAR || year > CALENDAR_MAX_YEAR || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;

    index = year - CALENDAR_MIN_YEAR;
    packed_date = ((uint32_t)year << 9) | ((uint32_t)month << 5) | day;
    if (calendar_new_year[index] > packed_date) {
        if (index == 0)
            return 0;
        index--;
    }

    packed_date = calendar_new_year[index];
    offset = solar_days(year, month, day) - solar_days(bits(packed_date, 12, 9), bits(packed_date, 4, 5), bits(packed_date, 5, 0)) + 1;
    months = calendar_lunar_months[index];
    lunar_month = 1;
    for (i = 0; i < 13; i++) {
        uint8_t month_days = (months & (0x1000U >> i)) ? 30 : 29;
        if (offset <= month_days)
            break;
        offset -= month_days;
        lunar_month++;
    }

    date->year = CALENDAR_MIN_YEAR + index;
    date->month = lunar_month;
    date->day = offset;
    date->is_leap_month = 0;
    i = bits(months, 4, 13);
    if (i && lunar_month > i) {
        date->month--;
        date->is_leap_month = date->month == i;
    }

    term = (month - 1) * 2;
    if (day == calendar_solar_term_days[year - CALENDAR_MIN_YEAR][term])
        date->solar_term = term;
    else if (day == calendar_solar_term_days[year - CALENDAR_MIN_YEAR][term + 1])
        date->solar_term = term + 1;
    else
        date->solar_term = CALENDAR_NO_SOLAR_TERM;
    return 1;
}
