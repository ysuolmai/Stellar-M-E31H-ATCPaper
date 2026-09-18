#!/usr/bin/env python3
"""Regenerate the compact lunar/solar-term tables and 16 px CJK glyphs."""

import datetime
import re
import urllib.request
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SOURCE = "https://raw.githubusercontent.com/iseemoon/lunar-calendar/89f66a4c1626674214bfcd077b300df951d60290"
FONT = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
GLYPHS = [
    ("ONE", "一"), ("TWO", "二"), ("THREE", "三"), ("FOUR", "四"),
    ("FIVE", "五"), ("SIX", "六"), ("SEVEN", "七"), ("EIGHT", "八"),
    ("NINE", "九"), ("TEN", "十"), ("ZHENG", "正"), ("DONG", "冬"),
    ("LA", "腊"), ("CHU", "初"), ("NIAN", "廿"), ("SANSHI", "卅"),
    ("RUN", "闰"), ("MONTH", "月"),
    ("RAT", "鼠"), ("OX", "牛"), ("TIGER", "虎"), ("RABBIT", "兔"),
    ("DRAGON", "龙"), ("SNAKE", "蛇"), ("HORSE", "马"), ("GOAT", "羊"),
    ("MONKEY", "猴"), ("ROOSTER", "鸡"), ("DOG", "狗"), ("PIG", "猪"),
    ("ZI", "子"), ("CHOU", "丑"), ("YIN", "寅"), ("MAO", "卯"),
    ("CHEN", "辰"), ("SI", "巳"), ("WU", "午"), ("WEI", "未"),
    ("SHEN", "申"), ("YOU", "酉"), ("XU", "戌"), ("HAI", "亥"),
    ("WEEK", "星"), ("PERIOD", "期"), ("SUN", "日"),
    ("SMALL", "小"), ("COLD", "寒"), ("BIG", "大"), ("BEGIN", "立"),
    ("SPRING", "春"), ("RAIN", "雨"), ("WATER", "水"), ("AWAKEN", "惊"),
    ("INSECTS", "蛰"), ("DIVIDE", "分"), ("CLEAR", "清"), ("BRIGHT", "明"),
    ("GRAIN", "谷"), ("SUMMER", "夏"), ("FULL", "满"), ("AWN", "芒"),
    ("SEED", "种"), ("ARRIVE", "至"), ("HEAT", "暑"), ("AUTUMN", "秋"),
    ("LIMIT", "处"), ("WHITE", "白"), ("DEW", "露"), ("FROST", "霜"),
    ("DESCEND", "降"), ("SNOW", "雪"),
]


def fetch(name):
    return urllib.request.urlopen(f"{SOURCE}/{name}").read().decode()


def table_values(source, name):
    body = re.search(rf"{name}\[\]\s*=\s*\{{(.*?)\}};", source, re.S).group(1)
    body = re.sub(r"/\*.*?\*/|//.*", "", body, flags=re.S)
    return [int(value, 0) for value in re.findall(r"0x[0-9a-fA-F]+|\d+", body)]


def write_calendar_data():
    lunar = fetch("lunar_date.c")
    solar = fetch("solar_term.c")
    new_year = table_values(lunar, "solar_1_1")
    month_days = table_values(lunar, "lunar_month_days")
    term_rows = {}
    for year, body in re.findall(r"\{\s*(20\d\d),\s*\{([^}]*)\}\s*\}", solar):
        stamps = [int(value) for value in re.findall(r"\d+", body)]
        zone = datetime.timezone(datetime.timedelta(hours=8))
        term_rows[int(year)] = [datetime.datetime.fromtimestamp(value / 1000, zone).day for value in stamps]

    start = 2000
    output = [
        "#pragma once", "", "#include <stdint.h>", "",
        "/* Calendar facts derived from github.com/iseemoon/lunar-calendar (Apache-2.0). */",
        "#define CALENDAR_MIN_YEAR 2000", "#define CALENDAR_MAX_YEAR 2099", "",
    ]
    for name, values in (("calendar_new_year", new_year[start - new_year[0]:2100 - new_year[0]]),
                         ("calendar_lunar_months", month_days[start - month_days[0]:2100 - month_days[0]])):
        output.append(f"static const uint32_t {name}[100] = {{")
        for index in range(0, 100, 10):
            output.append("    " + ", ".join(f"0x{value:x}" for value in values[index:index + 10]) + ",")
        output.extend(["};", ""])

    output.append("static const uint8_t calendar_solar_term_days[100][24] = {")
    for year in range(start, 2100):
        output.append("    {" + ",".join(str(day) for day in term_rows[year]) + f"}}, /* {year} */")
    output.extend(["};", ""])
    (ROOT / "Firmware/src/calendar_data.h").write_text("\n".join(output))


def write_font():
    font = ImageFont.truetype(FONT, 15, index=2)
    output = [
        "#pragma once", "", "#include <stdint.h>", "",
        "/* Generated from Noto Sans CJK SC Regular, SIL Open Font License 1.1. */",
        "enum calendar_glyph {",
    ]
    output.extend(f"    GLYPH_{name}," for name, _ in GLYPHS)
    output.extend(["    GLYPH_COUNT", "};", "", "static const uint8_t calendar_glyphs[GLYPH_COUNT][32] = {"])
    for name, char in GLYPHS:
        image = Image.new("1", (16, 16), 0)
        ImageDraw.Draw(image).text((0, -5), char, font=font, fill=1)
        packed = []
        for y in range(16):
            for half in range(2):
                value = 0
                for x in range(8):
                    value |= image.getpixel((half * 8 + x, y)) << (7 - x)
                packed.append(value)
        output.append(f"    /* {name} */ {{" + ",".join(f"0x{value:02x}" for value in packed) + "},")
    output.extend(["};", ""])
    (ROOT / "Firmware/src/calendar_font.h").write_text("\n".join(output))


if __name__ == "__main__":
    write_calendar_data()
    write_font()
