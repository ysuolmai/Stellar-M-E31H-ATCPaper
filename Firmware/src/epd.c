#include <stdint.h>
#include "etime.h"
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bw_213.h"
#include "epd_bwr_213.h"
#include "epd_bw_213_ice.h"
//#include "epd_bwr_154.h"
#include "epd_bwr_296.h"
#include "drivers.h"
#include "battery.h"
#include "calendar.h"
#include "calendar_font.h"

#include "OneBitDisplay.h"
#include "TIFF_G4.h"
extern const uint8_t ucMirror[];
#include "font_60.h"
#include "font16.h"
#include "font30.h"

RAM uint8_t epd_model = 0; // 0 = Undetected, 1 = BW213, 2 = BWR213, 3 = BWR154, 4 = BW213ICE, 5 BWR296
const char *epd_model_string[] = {"NC", "BW213", "BWR213", "BWR154", "213ICE", "BWR296"};
RAM uint8_t epd_update_state = 0;

RAM uint8_t epd_scene = 2;
RAM uint8_t epd_wait_update = 0;

RAM uint8_t hour_refresh = 100;
RAM uint8_t minute_refresh = 100;

RAM uint8_t epd_temperature_is_read = 0;
RAM uint8_t epd_temperature = 0;

uint8_t epd_buffer[epd_buffer_size];
uint8_t epd_temp[epd_buffer_size]; // for OneBitDisplay to draw into
RAM uint8_t epd_previous[epd_buffer_size];
RAM uint8_t epd_previous_valid = 0;
OBDISP obd;                        // virtual display structure
TIFFIMAGE tiff;

// With this we can force a display if it wasnt detected correctly
void set_EPD_model(uint8_t model_nr)
{
    epd_model = model_nr;
}

// With this we can force a display if it wasnt detected correctly
void set_EPD_scene(uint8_t scene)
{
    epd_scene = scene;
    set_EPD_wait_flush();
}

void set_EPD_wait_flush() {
    epd_wait_update = 1;
}



// Here we detect what E-Paper display is connected
_attribute_ram_code_ void EPD_detect_model(void)
{
    EPD_init();
    // system power
    EPD_POWER_ON();

    WaitMs(10);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    // Here we neeed to detect it
    if (EPD_BWR_296_detect())
    {
        epd_model = 5;
    }
    else if (EPD_BWR_213_detect())
    {
        epd_model = 2;
    }
//    else if (EPD_BWR_154_detect())// Right now this will never trigger, the 154 is same to 213BWR right now.
//    {
//        epd_model = 3;
//    }
    else if (EPD_BW_213_ice_detect())
    {
        epd_model = 4;
    }
    else
    {
        epd_model = 1;
    }
    epd_model = 5; // FIXME: only for bwr_296
    EPD_POWER_OFF();
}

_attribute_ram_code_ uint8_t EPD_read_temp(void)
{
    if (epd_temperature_is_read)
        return epd_temperature;

    if (!epd_model)
        EPD_detect_model();

    EPD_init();
    // system power
    EPD_POWER_ON();
    WaitMs(5);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    if (epd_model == 1)
        epd_temperature = EPD_BW_213_read_temp();
    else if (epd_model == 2)
        epd_temperature = EPD_BWR_213_read_temp();
//    else if (epd_model == 3)
//        epd_temperature = EPD_BWR_154_read_temp();
    else if (epd_model == 4)
        epd_temperature = EPD_BW_213_ice_read_temp();
    else if (epd_model == 5)
        epd_temperature = EPD_BWR_296_read_temp();

    EPD_POWER_OFF();

    epd_temperature_is_read = 1;

    return epd_temperature;
}

_attribute_ram_code_ void EPD_Display(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial)
{
    if (!epd_model)
        EPD_detect_model();

    EPD_init();
    // system power
    EPD_POWER_ON();
    WaitMs(5);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    if (epd_model == 1)
        epd_temperature = EPD_BW_213_Display(image, size, full_or_partial);
    else if (epd_model == 2)
        epd_temperature = EPD_BWR_213_Display(image, size, full_or_partial);
//    else if (epd_model == 3)
//        epd_temperature = EPD_BWR_154_Display(image, size, full_or_partial);
    else if (epd_model == 4)
        epd_temperature = EPD_BW_213_ice_Display(image, size, full_or_partial);
    else if (epd_model == 5) {
        uint8_t use_full_refresh = full_or_partial || !epd_previous_valid;
        epd_temperature = EPD_BWR_296_Display_BWR(image, red_image,
                                                   use_full_refresh ? NULL : epd_previous,
                                                   size, use_full_refresh);
        memcpy(epd_previous, image, size);
        epd_previous_valid = 1;
    }

    epd_temperature_is_read = 1;
    epd_update_state = 1;
}

_attribute_ram_code_ void epd_set_sleep(void)
{
    if (!epd_model)
        EPD_detect_model();

    if (epd_model == 1)
        EPD_BW_213_set_sleep();
    else if (epd_model == 2)
        EPD_BWR_213_set_sleep();
//    else if (epd_model == 3)
//        EPD_BWR_154_set_sleep();
    else if (epd_model == 4)
        EPD_BW_213_ice_set_sleep();
    else if (epd_model == 5)
        EPD_BWR_296_set_sleep();

    EPD_POWER_OFF();
    epd_update_state = 0;
}

_attribute_ram_code_ uint8_t epd_state_handler(void)
{
    switch (epd_update_state)
    {
    case 0:
        // Nothing todo
        break;
    case 1: // check if refresh is done and sleep epd if so
        if (epd_model == 1)
        {
            if (!EPD_IS_BUSY())
                epd_set_sleep();
        }
        else
        {
            if (EPD_IS_BUSY())
                epd_set_sleep();
        }
        break;
    }
    return epd_update_state;
}

_attribute_ram_code_ void FixBuffer(uint8_t *pSrc, uint8_t *pDst, uint16_t width, uint16_t height)
{
    int x, y;
    uint8_t *s, *d;
    for (y = 0; y < (height / 8); y++)
    { // byte rows
        d = &pDst[y];
        s = &pSrc[y * width];
        for (x = 0; x < width; x++)
        {
            d[x * (height / 8)] = ~ucMirror[s[width - 1 - x]]; // invert and flip
        }                                                      // for x
    }                                                          // for y
}

_attribute_ram_code_ void TIFFDraw(TIFFDRAW *pDraw)
{
    uint8_t uc = 0, ucSrcMask, ucDstMask, *s, *d;
    int x, y;

    s = pDraw->pPixels;
    y = pDraw->y;                          // current line
    d = &epd_buffer[(249 * 16) + (y / 8)]; // rotated 90 deg clockwise
    ucDstMask = 0x80 >> (y & 7);           // destination mask
    ucSrcMask = 0;                         // src mask
    for (x = 0; x < pDraw->iWidth; x++)
    {
        // Slower to draw this way, but it allows us to use a single buffer
        // instead of drawing and then converting the pixels to be the EPD format
        if (ucSrcMask == 0)
        { // load next source byte
            ucSrcMask = 0x80;
            uc = *s++;
        }
        if (!(uc & ucSrcMask))
        { // black pixel
            d[-(x * 16)] &= ~ucDstMask;
        }
        ucSrcMask >>= 1;
    }
}

_attribute_ram_code_ void epd_display_tiff(uint8_t *pData, int iSize)
{
    // test G4 decoder
    epd_clear();
    TIFF_openRAW(&tiff, 250, 122, BITDIR_MSB_FIRST, pData, iSize, TIFFDraw);
    TIFF_setDrawParameters(&tiff, 65536, TIFF_PIXEL_1BPP, 0, 0, 250, 122, NULL);
    TIFF_decode(&tiff);
    TIFF_close(&tiff);
    EPD_Display(epd_buffer, NULL, epd_buffer_size, 1);
}

extern uint8_t mac_public[6];
_attribute_ram_code_ void epd_display(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint8_t battery_level;

    if (epd_update_state)
        return;

    if (!epd_model)
    {
        EPD_detect_model();
    }
    uint16_t resolution_w = 250;
    uint16_t resolution_h = 128; // 122 real pixel, but needed to have a full byte
    if (epd_model == 1)
    {
        resolution_w = 250;
        resolution_h = 128; // 122 real pixel, but needed to have a full byte
    }
    else if (epd_model == 2)
    {
        resolution_w = 250;
        resolution_h = 128; // 122 real pixel, but needed to have a full byte
    }
    else if (epd_model == 3)
    {
        resolution_w = 200;
        resolution_h = 200;
    }
    else if (epd_model == 4)
    {
        resolution_w = 212;
        resolution_h = 104;
    }
    else if (epd_model == 5)
    {
        resolution_w = 296;
        resolution_h = 128;
    }

    epd_clear();

    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    battery_level = get_battery_level(battery_mv);
    sprintf(buff, "S24_%02X%02X%02X BW213", mac_public[2], mac_public[1], mac_public[0]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 46, 17, (char *)buff, 1);
    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    obdWriteStringCustom(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 100, 65, (char *)buff, 1);
    sprintf(buff, "-----%d'C-----", EPD_read_temp());
    obdWriteStringCustom(&obd, (GFXfont *)&Special_Elite_Regular_30, 46, 95, (char *)buff, 1);
    sprintf(buff, "Battery %dmV  %d%%", battery_mv, battery_level);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 46, 120, (char *)buff, 1);
    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);
    EPD_Display(epd_buffer, NULL, resolution_w * resolution_h / 8, full_or_partial);
}

_attribute_ram_code_ void epd_display_char(uint8_t data)
{
    int i;
    for (i = 0; i < epd_buffer_size; i++)
    {
        epd_buffer[i] = data;
    }
    EPD_Display(epd_buffer, NULL, epd_buffer_size, 1);
}

_attribute_ram_code_ void epd_clear(void)
{
    memset(epd_buffer, 0x00, epd_buffer_size);
    memset(epd_temp, 0x00, epd_buffer_size);
}
void update_time_scene(struct date_time _time, uint16_t battery_mv, int16_t temperature, void (*scene)(struct date_time, uint16_t, int16_t,  uint8_t)) {
    // default scene: show default time, battery, ble address, temperature
    if (epd_update_state)
        return;

    if (!epd_model)
    {
        EPD_detect_model();
    }

    if (epd_wait_update) {
        minute_refresh = _time.tm_min;
        scene(_time, battery_mv, temperature, 1);
        epd_wait_update = 0;
    }

    else if (_time.tm_min != minute_refresh)
    {
        uint8_t full_refresh = minute_refresh == 100 || _time.tm_min % 10 == 0;
        minute_refresh = _time.tm_min;
        // The first frame must be full; partial updates require a known panel image.
        scene(_time, battery_mv, temperature, full_refresh);
    }
}

void epd_update(struct date_time _time, uint16_t battery_mv, int16_t temperature) {
    switch(epd_scene) {
        case 1:
            update_time_scene(_time, battery_mv, temperature, epd_display);
            break;
        case 2:
            update_time_scene(_time, battery_mv, temperature, epd_display_time_with_date);
            break;
        default:
            break;
    }
}

static void draw_clock_digit(int x, int y, uint8_t digit) {
    static const uint8_t segments[] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    const int width = 51;
    const int height = 66;
    const int thickness = 8;
    const int middle = y + height / 2;
    uint8_t mask = segments[digit];

    if (mask & 0x01) obdRectangle(&obd, x + thickness, y, x + width - thickness - 1, y + thickness - 1, 1, 1);
    if (mask & 0x02) obdRectangle(&obd, x + width - thickness, y + thickness, x + width - 1, middle - 1, 1, 1);
    if (mask & 0x04) obdRectangle(&obd, x + width - thickness, middle, x + width - 1, y + height - thickness - 1, 1, 1);
    if (mask & 0x08) obdRectangle(&obd, x + thickness, y + height - thickness, x + width - thickness - 1, y + height - 1, 1, 1);
    if (mask & 0x10) obdRectangle(&obd, x, middle, x + thickness - 1, y + height - thickness - 1, 1, 1);
    if (mask & 0x20) obdRectangle(&obd, x, y + thickness, x + thickness - 1, middle - 1, 1, 1);
    if (mask & 0x40) obdRectangle(&obd, x + thickness, middle - thickness / 2, x + width - thickness - 1, middle + thickness / 2, 1, 1);
}

static void draw_clock_time(uint8_t hour, uint8_t minute) {
    draw_clock_digit(51, 29, hour / 10);
    draw_clock_digit(107, 29, hour % 10);
    obdRectangle(&obd, 163, 47, 170, 54, 1, 1);
    obdRectangle(&obd, 163, 73, 170, 80, 1, 1);
    draw_clock_digit(175, 29, minute / 10);
    draw_clock_digit(231, 29, minute % 10);
}

static int draw_calendar_glyph(int x, int y, uint8_t glyph)
{
    uint8_t row;
    uint8_t column;

    for (row = 0; row < 16; row++) {
        for (column = 0; column < 16; column++) {
            if (calendar_glyphs[glyph][row * 2 + column / 8] & (0x80 >> (column & 7))) {
                obdSetPixel(&obd, x + column, y + row, 1, 0);
                obdSetPixel(&obd, x + column + 1, y + row, 1, 0);
            }
        }
    }
    return x + 16;
}

static void draw_bold_text(GFXfont *font, int x, int y, char *text, uint8_t color)
{
    obdWriteStringCustom(&obd, font, x, y, text, color);
    obdWriteStringCustom(&obd, font, x + 1, y, text, color);
}

static int draw_lunar_date(int x, int y, const calendar_date_t *date)
{
    static const uint8_t numbers[] = {
        GLYPH_ONE, GLYPH_TWO, GLYPH_THREE, GLYPH_FOUR, GLYPH_FIVE,
        GLYPH_SIX, GLYPH_SEVEN, GLYPH_EIGHT, GLYPH_NINE, GLYPH_TEN
    };

    if (date->is_leap_month)
        x = draw_calendar_glyph(x, y, GLYPH_RUN);
    if (date->month == 1)
        x = draw_calendar_glyph(x, y, GLYPH_ZHENG);
    else if (date->month == 11)
        x = draw_calendar_glyph(x, y, GLYPH_DONG);
    else if (date->month == 12)
        x = draw_calendar_glyph(x, y, GLYPH_LA);
    else
        x = draw_calendar_glyph(x, y, numbers[date->month - 1]);
    x = draw_calendar_glyph(x, y, GLYPH_MONTH);

    if (date->day < 10) {
        x = draw_calendar_glyph(x, y, GLYPH_CHU);
        return draw_calendar_glyph(x, y, numbers[date->day - 1]);
    }
    if (date->day == 10) {
        x = draw_calendar_glyph(x, y, GLYPH_CHU);
        return draw_calendar_glyph(x, y, GLYPH_TEN);
    }
    if (date->day < 20) {
        x = draw_calendar_glyph(x, y, GLYPH_TEN);
        return draw_calendar_glyph(x, y, numbers[date->day - 11]);
    }
    if (date->day == 20) {
        x = draw_calendar_glyph(x, y, GLYPH_TWO);
        return draw_calendar_glyph(x, y, GLYPH_TEN);
    }
    if (date->day < 30) {
        x = draw_calendar_glyph(x, y, GLYPH_NIAN);
        return draw_calendar_glyph(x, y, numbers[date->day - 21]);
    }
    x = draw_calendar_glyph(x, y, GLYPH_THREE);
    return draw_calendar_glyph(x, y, GLYPH_TEN);
}

static int draw_solar_term(int x, int y, uint8_t term)
{
    static const uint8_t glyphs[24][2] = {
        {GLYPH_SMALL, GLYPH_COLD}, {GLYPH_BIG, GLYPH_COLD},
        {GLYPH_BEGIN, GLYPH_SPRING}, {GLYPH_RAIN, GLYPH_WATER},
        {GLYPH_AWAKEN, GLYPH_INSECTS}, {GLYPH_SPRING, GLYPH_DIVIDE},
        {GLYPH_CLEAR, GLYPH_BRIGHT}, {GLYPH_GRAIN, GLYPH_RAIN},
        {GLYPH_BEGIN, GLYPH_SUMMER}, {GLYPH_SMALL, GLYPH_FULL},
        {GLYPH_AWN, GLYPH_SEED}, {GLYPH_SUMMER, GLYPH_ARRIVE},
        {GLYPH_SMALL, GLYPH_HEAT}, {GLYPH_BIG, GLYPH_HEAT},
        {GLYPH_BEGIN, GLYPH_AUTUMN}, {GLYPH_LIMIT, GLYPH_HEAT},
        {GLYPH_WHITE, GLYPH_DEW}, {GLYPH_AUTUMN, GLYPH_DIVIDE},
        {GLYPH_COLD, GLYPH_DEW}, {GLYPH_FROST, GLYPH_DESCEND},
        {GLYPH_BEGIN, GLYPH_DONG}, {GLYPH_SMALL, GLYPH_SNOW},
        {GLYPH_BIG, GLYPH_SNOW}, {GLYPH_DONG, GLYPH_ARRIVE}
    };

    x = draw_calendar_glyph(x, y, glyphs[term][0]);
    return draw_calendar_glyph(x, y, glyphs[term][1]);
}

void epd_display_time_with_date(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial) {
    uint16_t battery_level;
    calendar_date_t date;
    int x;
    int text_width;
    int text_top;
    int text_bottom;
    int16_t display_temperature;

    epd_clear();

    obdCreateVirtualDisplay(&obd, epd_width, epd_height, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    battery_level = get_battery_level(battery_mv);

    draw_clock_time(_time.tm_hour, _time.tm_min);

    sprintf(buff, "%d-%02d-%02d", _time.tm_year, _time.tm_month, _time.tm_day);
    draw_bold_text((GFXfont *)&Dialog_plain_16, 49, 20, buff, 1);
    x = draw_calendar_glyph(178, 7, GLYPH_WEEK);
    x = draw_calendar_glyph(x, 7, GLYPH_PERIOD);
    draw_calendar_glyph(x, 7, (_time.tm_week == 0 || _time.tm_week == 7) ? GLYPH_SUN : GLYPH_ONE + _time.tm_week - 1);

    obdRectangle(&obd, 247, 7, 250, 12, 1, 1);
    obdRectangle(&obd, 251, 2, 292, 18, 1, 0);
    sprintf(buff, "%d", battery_level);
    draw_bold_text((GFXfont *)&Dialog_plain_16,
                   battery_level >= 100 ? 256 : (battery_level >= 10 ? 261 : 267),
                   16, buff, 1);

    if (calendar_get(_time.tm_year, _time.tm_month, _time.tm_day, &date)) {
        static const uint8_t branches[] = {
            GLYPH_ZI, GLYPH_CHOU, GLYPH_YIN, GLYPH_MAO, GLYPH_CHEN, GLYPH_SI,
            GLYPH_WU, GLYPH_WEI, GLYPH_SHEN, GLYPH_YOU, GLYPH_XU, GLYPH_HAI
        };
        static const uint8_t animals[] = {
            GLYPH_RAT, GLYPH_OX, GLYPH_TIGER, GLYPH_RABBIT, GLYPH_DRAGON, GLYPH_SNAKE,
            GLYPH_HORSE, GLYPH_GOAT, GLYPH_MONKEY, GLYPH_ROOSTER, GLYPH_DOG, GLYPH_PIG
        };
        uint8_t zodiac = (date.year - 4) % 12;

        draw_calendar_glyph(61, 104, branches[zodiac]);
        draw_calendar_glyph(77, 104, animals[zodiac]);
        draw_lunar_date(date.is_leap_month ? 99 : 107, 104, &date);
        draw_solar_term(185, 104, date.solar_term);
        display_temperature = temperature;
        sprintf(buff, "%d'C", display_temperature);
        obdGetStringBox((GFXfont *)&Dialog_plain_16, buff, &text_width, &text_top, &text_bottom);
        draw_bold_text((GFXfont *)&Dialog_plain_16, 264 - (text_width + 1) / 2, 119, buff, 1);
    }

    FixBuffer(epd_temp, epd_buffer, epd_width, epd_height);

    EPD_Display(epd_buffer, NULL, epd_width * epd_height / 8, full_or_partial);
}
