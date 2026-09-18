#pragma once

#include <stdint.h>

#define CALENDAR_NO_SOLAR_TERM 0xff

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t is_leap_month;
    uint8_t solar_term;
} calendar_date_t;

uint8_t calendar_get(uint16_t year, uint8_t month, uint8_t day, calendar_date_t *date);
