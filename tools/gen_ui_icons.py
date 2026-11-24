#!/usr/bin/env python3

import math
from functools import partial

from gen_icons import hex_rgb, mix_rgb, sd_circle, sd_round_box, sd_segment, write_assets

AMBER = (0xFFF2E0, 0xB9610E, 0xF29838)
CYAN = (0xE9F4FF, 0x1D6EC0, 0x46A0F2)
VIOLET = (0xF2EEFF, 0x634BD6, 0x8D79F2)
MINT = (0xE6F8F3, 0x177E65, 0x2EB897)
ORANGE = (0xFFF0E4, 0xC55412, 0xF4823C)
CORAL = (0xFFEDF0, 0xC73E52, 0xF26D7E)
HALF_STROKE = 1.7
GLYPH_HALF_SIZE = 32


def sd_polygon(x, y, vertices):
    inside = False
    distance = math.inf
    for (ax, ay), (bx, by) in zip(vertices, vertices[1:] + vertices[:1]):
        distance = min(distance, sd_segment(x, y, ax, ay, bx, by))
        if (ay > y) != (by > y) and x < ax + (bx - ax) * (y - ay) / (by - ay):
            inside = not inside
    return -distance if inside else distance


def distance_to_arc(x, y, cx, cy, radius, span, heading=0.0):
    angle = math.remainder(math.atan2(y - cy, x - cx) - heading, math.tau)
    angle = heading + max(-span, min(span, angle))
    return math.hypot(x - cx - radius * math.cos(angle), y - cy - radius * math.sin(angle))


RAYS = [
    (32 + 14.5 * math.cos(a), 32 + 14.5 * math.sin(a),
     32 + 19 * math.cos(a), 32 + 19 * math.sin(a))
    for a in (i * math.pi / 4 for i in range(8))
]


def sun(x, y):
    disk = sd_circle(x, y, 32, 32, 9.5)
    rays = min(sd_segment(x, y, *ray) for ray in RAYS)
    return disk, min(abs(disk), rays)


def speaker(x, y):
    body = sd_polygon(x, y, ((15, 28), (24, 28), (34, 20), (34, 44), (24, 36), (15, 36)))
    waves = min(distance_to_arc(x, y, 33, 32, 10, 0.74), distance_to_arc(x, y, 33, 32, 17, 0.74))
    return body, min(abs(body), waves)


def clock(x, y):
    face = sd_circle(x, y, 32, 32, 16.5)
    hands = min(sd_segment(x, y, 32, 22.5, 32, 32), sd_segment(x, y, 32, 32, 39, 36))
    return face, min(abs(face), hands)


def info(x, y):
    screen = sd_round_box(x - 32, y - 32, 16, 16, 9)
    dot = sd_circle(x, y, 32, 26, 2.4)
    stem = sd_segment(x, y, 32, 32, 32, 40)
    return screen, min(abs(screen), dot + HALF_STROKE, stem)


def bowl(x, y):
    body = max(sd_circle(x, y, 32, 30, 17), 32 - y)
    sticks = min(sd_segment(x, y, 22, 25, 43, 15), sd_segment(x, y, 28, 27, 48, 20))
    return body, min(abs(body), sticks)


def book(x, y):
    pages = sd_polygon(x, y, ((14, 22), (21, 19), (26, 19), (32, 22), (38, 19),
                           (43, 19), (50, 22), (50, 44), (42, 42), (32, 46),
                           (22, 42), (14, 44)))
    star = sd_polygon(x, y, ((32, 24), (34, 30), (40, 32), (34, 34),
                          (32, 40), (30, 34), (24, 32), (30, 30)))
    return pages, min(abs(pages), star + HALF_STROKE)


def coin(x, y):
    outer = sd_circle(x, y, 32, 32, 18.5)
    inner = sd_circle(x, y, 32, 32, 12.5)
    hole = sd_round_box(x - 32, y - 32, 4, 4, 1.4)
    return max(outer, -hole), min(abs(outer), abs(inner), abs(hole))


def dice(x, y):
    body = sd_round_box(x - 32, y - 32, 16, 16, 7)
    pips = min(sd_circle(x, y, 24 + 8 * i, 40 - 8 * i, 2.7) for i in range(3))
    return body, min(abs(body), pips + HALF_STROKE)


def number(x, y):
    strokes = min(sd_segment(x, y, 30, 17, 24, 47), sd_segment(x, y, 42, 17, 36, 47),
                  sd_segment(x, y, 18, 27, 47, 27), sd_segment(x, y, 16, 37, 45, 37))
    return math.inf, strokes


def sunset(x, y):
    sun_disk = max(sd_circle(x, y, 32, 40, 11), y - 40)
    strokes = min(
        abs(sun_disk), sd_segment(x, y, 16, 40, 48, 40),
        sd_segment(x, y, 32, 16, 32, 22),
        sd_segment(x, y, 19, 22, 23, 26),
        sd_segment(x, y, 45, 22, 41, 26),
    )
    return sun_disk, strokes


def pomodoro(x, y):
    face = sd_circle(x, y, 32, 35, 15)
    strokes = min(
        abs(face), sd_segment(x, y, 27, 15, 37, 15),
        sd_segment(x, y, 32, 15, 32, 20),
        sd_segment(x, y, 32, 26, 32, 35),
        sd_segment(x, y, 32, 35, 39, 39),
    )
    return face, strokes


def digital_clock(x, y):
    frame = sd_round_box(x - 32, y - 32, 19, 12, 7)
    zero = sd_round_box(x - 41, y - 32, 3.5, 5, 2)
    colon = min(sd_circle(x, y, 32, 28.5, 2), sd_circle(x, y, 32, 35.5, 2))
    one = min(sd_segment(x, y, 21, 27, 25, 27), sd_segment(x, y, 25, 27, 25, 37))
    return frame, min(abs(frame), abs(zero), colon + HALF_STROKE, one)


def power_off(x, y):
    arc = distance_to_arc(y, x, 34, 32, 15, 2.43)
    return math.inf, min(arc, sd_segment(x, y, 32, 16, 32, 30))


def reboot(x, y):
    center_x, center_y, radius = 32, 35, 15
    tip_x, tip_y = 40, center_y - radius
    arc = distance_to_arc(x, y, center_x, center_y, radius, math.radians(150), heading=math.radians(120))
    stem = sd_segment(x, y, center_x, tip_y, tip_x, tip_y)
    head = min(sd_segment(x, y, tip_x, tip_y, 33.5, 13.5),
               sd_segment(x, y, tip_x, tip_y, 33.5, 26.5))
    return math.inf, min(arc, stem, head)


def key_back(x, y):
    frame = sd_round_box(x - 32, y - 32, 16, 13, 8)
    arrow = min(sd_segment(x, y, 29, 27, 23, 32), sd_segment(x, y, 23, 32, 29, 37),
                sd_segment(x, y, 24, 32, 37, 32), sd_segment(x, y, 37, 32, 41, 36))
    return frame, min(abs(frame), arrow)


def key_spark(x, y):
    star = sd_polygon(x, y, ((32, 15), (36.6, 27.4), (49, 32), (36.6, 36.6),
                            (32, 49), (27.4, 36.6), (15, 32), (27.4, 27.4)))
    return star, abs(star)


def draw_icon(nx, ny, glyph, palette, outline=False):
    if sd_round_box(nx, ny, 0.97, 0.97, 0.56) > 0:
        return (0.0, 0.0, 0.0, 0.0)
    background, foreground, accent = palette
    fill, stroke = glyph((nx + 1) * GLYPH_HALF_SIZE, (ny + 1) * GLYPH_HALF_SIZE)
    if stroke <= HALF_STROKE:
        return (*foreground, 1.0)
    if outline:
        return (0.0, 0.0, 0.0, 0.0)
    return (*(mix_rgb(background, accent, 0.29) if fill <= 0 else background), 1.0)


ICONS = (
    ("db_icon_set_display", 52, sun, AMBER),
    ("db_icon_set_sound", 52, speaker, CYAN),
    ("db_icon_set_time", 52, clock, VIOLET),
    ("db_icon_set_about", 52, info, MINT),
    ("db_icon_rl_eat", 64, bowl, ORANGE),
    ("db_icon_rl_answers", 46, book, VIOLET),
    ("db_icon_rl_coin", 46, coin, AMBER),
    ("db_icon_rl_dice", 46, dice, MINT),
    ("db_icon_rl_number", 46, number, CYAN),
    ("db_icon_tt_offwork", 48, sunset, MINT),
    ("db_icon_tt_pomo", 52, pomodoro, CORAL),
    ("db_icon_tt_clock", 52, digital_clock, CYAN),
    ("db_icon_pwr_off", 52, power_off, CORAL),
    ("db_icon_pwr_reboot", 52, reboot, AMBER),
    ("db_icon_key_back", 52, key_back, CYAN),
    ("db_icon_key_spark", 52, key_spark, AMBER),
)


if __name__ == "__main__":
    write_assets(
        "db_ui_icons",
        [(name, size, partial(draw_icon, glyph=glyph, palette=tuple(map(hex_rgb, palette)),
                              outline=glyph in (power_off, reboot)))
         for name, size, glyph, palette in ICONS],
        "Auto-generated duotone UI icons for Desk Buddy (tools/gen_ui_icons.py)",
    )
