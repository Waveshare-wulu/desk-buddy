#!/usr/bin/env python3

from __future__ import annotations

import math
from pathlib import Path

SIZE = 100
SUPERSAMPLE = 3


def clamp(v: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return lo if v < lo else (hi if v > hi else v)


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    t = clamp((x - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)


def mix(a: float, b: float, t: float) -> float:
    return a * (1.0 - t) + b * t


def mix_rgb(c1: tuple[float, float, float], c2: tuple[float, float, float], t: float):
    return (mix(c1[0], c2[0], t), mix(c1[1], c2[1], t), mix(c1[2], c2[2], t))


def hex_rgb(h: int) -> tuple[float, float, float]:
    return (((h >> 16) & 0xFF) / 255.0, ((h >> 8) & 0xFF) / 255.0, (h & 0xFF) / 255.0)


def sd_round_box(px: float, py: float, bx: float, by: float, r: float) -> float:
    qx = abs(px) - bx + r
    qy = abs(py) - by + r
    return math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - r


def sd_circle(px: float, py: float, cx: float, cy: float, r: float) -> float:
    return math.hypot(px - cx, py - cy) - r


def sd_segment(px: float, py: float, ax: float, ay: float, bx: float, by: float) -> float:
    pax, pay = px - ax, py - ay
    bax, bay = bx - ax, by - ay
    denom = bax * bax + bay * bay
    h = clamp((pax * bax + pay * bay) / denom) if denom > 1e-9 else 0.0
    return math.hypot(pax - bax * h, pay - bay * h)


def blend_over(dst, src):
    sr, sg, sb, sa = src
    dr, dg, db, da = dst
    out_a = sa + da * (1.0 - sa)
    if out_a < 1e-6:
        return (0.0, 0.0, 0.0, 0.0)
    r = (sr * sa + dr * da * (1.0 - sa)) / out_a
    g = (sg * sa + dg * da * (1.0 - sa)) / out_a
    b = (sb * sa + db * da * (1.0 - sa)) / out_a
    return (r, g, b, out_a)


def pastel_squircle_bg(nx: float, ny: float, top_hex: int, bot_hex: int, rim_hex: int = 0xFFFFFF):
    d_sh = sd_round_box(nx, ny - 0.045, 0.87, 0.87, 0.44)
    sh_a = (1.0 - smoothstep(-0.04, 0.08, d_sh)) * 0.14
    out = (0.60, 0.48, 0.34, sh_a)

    d = sd_round_box(nx, ny + 0.015, 0.88, 0.88, 0.46)
    alpha = 1.0 - smoothstep(-0.022, 0.022, d)
    if alpha <= 0.0:
        return out

    t = clamp((nx * 0.35 + ny * 0.65 + 0.9) / 1.8)
    col = mix_rgb(hex_rgb(top_hex), hex_rgb(bot_hex), t)

    rim = smoothstep(-0.075, -0.025, d) * (1.0 - smoothstep(-0.015, 0.015, d)) * 0.88
    col = mix_rgb(col, hex_rgb(rim_hex), rim)

    return blend_over(out, (col[0], col[1], col[2], alpha))


def draw_random_lab(nx: float, ny: float):
    col = pastel_squircle_bg(nx, ny, 0xF5F1FF, 0xE3DAFF)
    if col[3] <= 0.0:
        return col
    ang = -0.18
    rx = nx * math.cos(ang) - (ny + 0.01) * math.sin(ang)
    ry = nx * math.sin(ang) + (ny + 0.01) * math.cos(ang)
    die = sd_round_box(rx, ry, 0.44, 0.44, 0.18)
    a_die = 1.0 - smoothstep(-0.022, 0.022, die)
    die_c = mix_rgb(hex_rgb(0x9A84FF), hex_rgb(0x765EF0), clamp((ry + 0.45) / 0.9))
    col = blend_over(col, (die_c[0], die_c[1], die_c[2], a_die))
    for px, py, phex in [
        (-0.20, -0.20, 0xFFFFFF),
        (0.20, -0.20, 0xFFFFFF),
        (0.0, 0.0, 0xFFE680),
        (-0.20, 0.20, 0xFFFFFF),
        (0.20, 0.20, 0xFFFFFF),
    ]:
        pip = sd_circle(rx, ry, px, py, 0.072)
        ap = 1.0 - smoothstep(-0.02, 0.02, pip)
        pc = hex_rgb(phex)
        col = blend_over(col, (pc[0], pc[1], pc[2], ap * a_die))
    sx, sy = nx - 0.50, ny + 0.46
    star = (abs(sx) ** 0.65 + abs(sy) ** 0.65) - 0.17 ** 0.65
    as_ = 1.0 - smoothstep(-0.02, 0.02, star)
    sc = hex_rgb(0xFFB830)
    col = blend_over(col, (sc[0], sc[1], sc[2], as_))
    return col


def cubic_points(start, control_a, control_b, end, steps):
    return [
        tuple(
            (1 - t) ** 3 * a + 3 * (1 - t) ** 2 * t * b + 3 * (1 - t) * t ** 2 * c + t ** 3 * d
            for a, b, c, d in zip(start, control_a, control_b, end)
        )
        for t in (step / steps for step in range(steps + 1))
    ]


def curve_segments(start, *curves, steps=8):
    segments = []
    for control_a, control_b, end in curves:
        points = cubic_points(start, control_a, control_b, end, steps)
        segments.extend((*a, *b) for a, b in zip(points, points[1:]))
        start = end
    return segments


def sd_closed_path(x, y, segments):
    inside = False
    distance = math.inf
    for ax, ay, bx, by in segments:
        distance = min(distance, sd_segment(x, y, ax, ay, bx, by))
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return -distance if inside else distance


def sd_ellipse(x, y, cx, cy, rx, ry):
    return (math.hypot((x - cx) / rx, (y - cy) / ry) - 1.0) * min(rx, ry)


def paint(dst, rgb, distance, opacity=1.0):
    alpha = clamp(0.5 - distance * 2.0) * opacity
    return blend_over(dst, (*rgb, alpha)) if alpha else dst


_FISH_OUTLINE = curve_segments(
    (22, 57),
    ((22, 39), (36, 33), (52, 34)),
    ((68, 35), (78, 44), (78, 56)),
    ((78, 66), (69, 72), (51, 72)),
    ((34, 72), (22, 68), (22, 57)),
    steps=12,
)
_FISH_DETAIL = (
    (curve_segments((33, 41), ((40, 37.5), (52, 37.5), (61, 41))), 1.65, 0xFFF4DC, 0.72),
    (curve_segments((29, 53), ((32, 50.5), (35, 50.5), (37.5, 53))), 0.9, 0xB65B14, 0.45),
    (curve_segments((31, 61), ((34, 59), (37, 59), (39.5, 61))), 0.9, 0xB65B14, 0.45),
)
_FISH_SLOT = curve_segments((45, 57.5), ((54, 59.5), (66, 57.6), (75.3, 55.8)))


def draw_fish(x, y, dst, rim=0.0):
    shadow = math.hypot((x - 50) / 30, (y - 81) / 2.35)
    dst = blend_over(dst, (*hex_rgb(0xAD815C), (1.0 - smoothstep(0.32, 1.0, shadow)) * 0.22))
    pad = mix_rgb(hex_rgb(0xC98A4E), hex_rgb(0xA0622E), clamp((y - 70) / 10))
    dst = paint(dst, pad, sd_round_box(x - 50, y - 75, 25, 5, 2.6))
    dst = paint(dst, hex_rgb(0xE9B984), sd_round_box(x - 50, y - 72.6, 21.5, 1.1, 1.1), 0.7)

    if not (21 <= x <= 79 and 33 <= y <= 73):
        return dst
    body = sd_closed_path(x, y, _FISH_OUTLINE)
    wood = mix_rgb(hex_rgb(0xF6B05B), hex_rgb(0xD97925), clamp((y - 34) / 38))
    if rim:
        wood = mix_rgb(wood, hex_rgb(0xFFE2B5), smoothstep(-rim - 0.25, -rim + 0.25, body))
    dst = paint(dst, wood, body)
    if body >= 0:
        return dst

    for segments, radius, color, opacity in _FISH_DETAIL:
        distance = min(sd_segment(x, y, *segment) for segment in segments) - radius
        dst = paint(dst, hex_rgb(color), distance, opacity)
    slot = min(
        sd_segment(x, y, *segment) - mix(2.1, 0.95, (i + 0.5) / len(_FISH_SLOT))
        for i, segment in enumerate(_FISH_SLOT)
    )
    opening = min(slot, sd_circle(x, y, 45, 57.5, 4.5))
    return paint(dst, hex_rgb(0x6E3510), max(body, opening))


def draw_mallet(x, y, dst):
    handle = sd_segment(x, y, 28, 32, 123, 13) - 5
    dst = paint(dst, hex_rgb(0xA6581C), handle)

    angle = math.radians(14)
    dx, dy = x - 24, y - 33
    u = dx * math.cos(angle) - dy * math.sin(angle)
    v = dx * math.sin(angle) + dy * math.cos(angle)
    dst = paint(dst, hex_rgb(0x7A3E14), sd_ellipse(u, v, 0, 0, 19, 14))
    return paint(dst, hex_rgb(0xC98A52), sd_ellipse(u, v, -4, -4, 8, 3.8), 0.65)


def draw_wooden_fish(nx: float, ny: float):
    x, y = (nx + 1) * 50, (ny + 1) * 50
    dst = pastel_squircle_bg(nx, ny, 0xFFF6E6, 0xFFE4BE)
    dst = draw_fish(x, y, dst)
    return draw_mallet((x - 44) / 0.31, (y - 14) / 0.42, dst)


def draw_daily_fortune(nx: float, ny: float):
    col = pastel_squircle_bg(nx, ny, 0xFFF0F2, 0xFFD6DE)
    if col[3] <= 0.0:
        return col
    cord = abs(sd_circle(nx, ny, 0.0, -0.45, 0.20)) - 0.038
    a_cord = (1.0 - smoothstep(-0.02, 0.02, cord)) * (1.0 if ny < -0.44 else 0.0)
    cc = hex_rgb(0xF0A535)
    col = blend_over(col, (cc[0], cc[1], cc[2], a_cord))

    pouch = sd_round_box(nx, ny - 0.04, 0.41, 0.51, 0.17)
    a_p = 1.0 - smoothstep(-0.022, 0.022, pouch)
    pc = mix_rgb(hex_rgb(0xFF8396), hex_rgb(0xEC566F), clamp((ny + 0.5) / 1.0))
    col = blend_over(col, (pc[0], pc[1], pc[2], a_p))

    tag = sd_round_box(nx, ny - 0.04, 0.25, 0.34, 0.10)
    a_t = 1.0 - smoothstep(-0.02, 0.02, tag)
    tc = hex_rgb(0xFFF9F2)
    col = blend_over(col, (tc[0], tc[1], tc[2], a_t))

    ax, ay = abs(nx), abs(ny - 0.04)
    star = (ax ** 0.65 + ay ** 0.65) - 0.22 ** 0.65
    a_s = 1.0 - smoothstep(-0.025, 0.025, star)
    sc = hex_rgb(0xF26D7E)
    col = blend_over(col, (sc[0], sc[1], sc[2], a_s))

    dot = sd_circle(nx, ny, 0.54, -0.46, 0.075)
    a_d = 1.0 - smoothstep(-0.02, 0.02, dot)
    dc = hex_rgb(0xFFB940)
    col = blend_over(col, (dc[0], dc[1], dc[2], a_d))
    return col


def draw_game_2048(nx: float, ny: float):
    col = pastel_squircle_bg(nx, ny, 0xFFF3E6, 0xFFDCBE)
    if col[3] <= 0.0:
        return col
    tiles = [
        (-0.29, -0.29, 0xFFFFFF),
        (0.29, -0.29, 0xFFC96B),
        (-0.29, 0.29, 0xFF9E62),
        (0.29, 0.29, 0xF27340),
    ]
    for cx, cy, thex in tiles:
        d = sd_round_box(nx - cx, ny - cy, 0.24, 0.24, 0.10)
        a = 1.0 - smoothstep(-0.02, 0.02, d)
        tc = hex_rgb(thex)
        col = blend_over(col, (tc[0], tc[1], tc[2], a))
    for cx, cy, thex, r in [
        (-0.29, -0.29, 0xE5B88E, 0.065),
        (0.29, -0.29, 0xFFFFFF, 0.065),
        (-0.29, 0.29, 0xFFFFFF, 0.065),
        (0.29, 0.29, 0xFFF4C2, 0.095),
    ]:
        dot = sd_circle(nx, ny, cx, cy, r)
        ad = 1.0 - smoothstep(-0.02, 0.02, dot)
        dc = hex_rgb(thex)
        col = blend_over(col, (dc[0], dc[1], dc[2], ad))
    return col


def draw_relative_calc(nx: float, ny: float):
    col = pastel_squircle_bg(nx, ny, 0xE7FAF4, 0xC4F0E3)
    if col[3] <= 0.0:
        return col
    l1 = sd_segment(nx, ny, 0.0, -0.32, -0.35, 0.28) - 0.048
    l2 = sd_segment(nx, ny, 0.0, -0.32, 0.35, 0.28) - 0.048
    l3 = sd_segment(nx, ny, -0.35, 0.28, 0.35, 0.28) - 0.036
    al = (1.0 - smoothstep(-0.02, 0.02, min(l1, l2, l3))) * 0.65
    lc = hex_rgb(0x2EB897)
    col = blend_over(col, (lc[0], lc[1], lc[2], al))

    top_o = sd_circle(nx, ny, 0.0, -0.32, 0.22)
    col = blend_over(col, (*hex_rgb(0x2EB897), 1.0 - smoothstep(-0.02, 0.02, top_o)))
    top_i = sd_circle(nx, ny, 0.0, -0.32, 0.09)
    col = blend_over(col, (1.0, 1.0, 1.0, 1.0 - smoothstep(-0.02, 0.02, top_i)))

    for cx in (-0.36, 0.36):
        bo = sd_circle(nx, ny, cx, 0.29, 0.20)
        col = blend_over(col, (*hex_rgb(0x2EB897), 1.0 - smoothstep(-0.02, 0.02, bo)))
        bi = sd_circle(nx, ny, cx, 0.29, 0.13)
        col = blend_over(col, (1.0, 1.0, 1.0, 1.0 - smoothstep(-0.02, 0.02, bi)))
    return col


def draw_time_tools(nx: float, ny: float):
    col = pastel_squircle_bg(nx, ny, 0xEAF6FF, 0xCAE6FF)
    if col[3] <= 0.0:
        return col
    for ex in (-0.38, 0.38):
        ear = sd_circle(nx, ny, ex, -0.36, 0.16)
        col = blend_over(col, (*hex_rgb(0x4CA4F4), 1.0 - smoothstep(-0.02, 0.02, ear)))
    ring = sd_circle(nx, ny, 0.0, 0.06, 0.52)
    col = blend_over(col, (*hex_rgb(0x46A0F2), 1.0 - smoothstep(-0.02, 0.02, ring)))
    dial = sd_circle(nx, ny, 0.0, 0.06, 0.43)
    col = blend_over(col, (1.0, 1.0, 1.0, 1.0 - smoothstep(-0.02, 0.02, dial)))
    h_hand = sd_segment(nx, ny, 0.0, 0.06, 0.0, -0.22) - 0.044
    m_hand = sd_segment(nx, ny, 0.0, 0.06, 0.23, 0.18) - 0.040
    col = blend_over(col, (*hex_rgb(0x2D78C9), 1.0 - smoothstep(-0.02, 0.02, min(h_hand, m_hand))))
    pin = sd_circle(nx, ny, 0.0, 0.06, 0.075)
    col = blend_over(col, (*hex_rgb(0xF26D7E), 1.0 - smoothstep(-0.02, 0.02, pin)))
    return col


def draw_settings(nx: float, ny: float):
    col = pastel_squircle_bg(nx, ny, 0xF3F0EB, 0xE2DDD4)
    if col[3] <= 0.0:
        return col
    r = math.hypot(nx, ny + 0.01)
    ang = math.atan2(ny + 0.01, nx)
    tooth = 0.075 * math.cos(8.0 * ang)
    gear_outer = r - (0.43 + tooth)
    gear_inner = 0.21 - r
    gear = max(gear_outer, gear_inner)
    col = blend_over(col, (*hex_rgb(0x7A7269), 1.0 - smoothstep(-0.024, 0.024, gear)))
    hole = sd_circle(nx, ny, 0.0, -0.01, 0.16)
    col = blend_over(col, (1.0, 1.0, 1.0, 1.0 - smoothstep(-0.02, 0.02, hole)))
    return col


ICONS = [
    ("db_icon_random_lab", draw_random_lab),
    ("db_icon_wooden_fish", draw_wooden_fish),
    ("db_icon_daily_fortune", draw_daily_fortune),
    ("db_icon_game_2048", draw_game_2048),
    ("db_icon_relative_calc", draw_relative_calc),
    ("db_icon_time_tools", draw_time_tools),
    ("db_icon_settings", draw_settings),
]


def image_dimensions(size):
    return (size, size) if isinstance(size, int) else size


def render_icon(draw_fn, size=SIZE) -> bytes:
    width, height = image_dimensions(size)
    # LVGL v9 LV_COLOR_FORMAT_ARGB8888 stores B, G, R, A byte order on little-endian
    out = bytearray(width * height * 4)
    inv_ss = 1.0 / (SUPERSAMPLE * SUPERSAMPLE)
    for y in range(height):
        for x in range(width):
            r_sum = g_sum = b_sum = a_sum = 0.0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    px = (x + (sx + 0.5) / SUPERSAMPLE) / width * 2.0 - 1.0
                    py = (y + (sy + 0.5) / SUPERSAMPLE) / height * 2.0 - 1.0
                    r, g, b, a = draw_fn(px, py)
                    r_sum += r * a
                    g_sum += g * a
                    b_sum += b * a
                    a_sum += a
            a_avg = clamp(a_sum * inv_ss)
            if a_avg > 1e-4:
                r_avg = clamp((r_sum * inv_ss) / a_avg)
                g_avg = clamp((g_sum * inv_ss) / a_avg)
                b_avg = clamp((b_sum * inv_ss) / a_avg)
            else:
                r_avg = g_avg = b_avg = 0.0
            idx = (y * width + x) * 4
            out[idx + 0] = int(round(b_avg * 255))
            out[idx + 1] = int(round(g_avg * 255))
            out[idx + 2] = int(round(r_avg * 255))
            out[idx + 3] = int(round(a_avg * 255))
    return bytes(out)


def write_assets(stem, icons, description) -> None:
    root = Path(__file__).resolve().parent.parent
    assets_dir = root / "main" / "ui" / "assets"
    assets_dir.mkdir(parents=True, exist_ok=True)

    header_lines = [
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
    ]
    for name, _, _ in icons:
        header_lines.append(f"LV_IMAGE_DECLARE({name});")
    header_lines.extend(["", "#ifdef __cplusplus", "}", "#endif", ""])
    (assets_dir / f"{stem}.h").write_text("\n".join(header_lines), encoding="utf-8")

    c_lines = [
        f"/* {description} */",
        f'#include "{stem}.h"',
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
    ]
    for name, size, fn in icons:
        width, height = image_dimensions(size)
        data = render_icon(fn, size)
        c_lines.append(f"static const LV_ATTRIBUTE_MEM_ALIGN uint8_t {name}_map[] = {{")
        for i in range(0, len(data), 16):
            chunk = ", ".join(f"0x{b:02x}" for b in data[i : i + 16])
            c_lines.append(f"    {chunk},")
        c_lines.append("};")
        c_lines.append("")
        c_lines.append(f"const lv_image_dsc_t {name} = {{")
        c_lines.append("    .header = {")
        c_lines.append("        .magic = LV_IMAGE_HEADER_MAGIC,")
        c_lines.append("        .cf = LV_COLOR_FORMAT_ARGB8888,")
        c_lines.append("        .flags = 0,")
        c_lines.append(f"        .w = {width},")
        c_lines.append(f"        .h = {height},")
        c_lines.append(f"        .stride = {width * 4},")
        c_lines.append("    },")
        c_lines.append(f"    .data_size = sizeof({name}_map),")
        c_lines.append(f"    .data = {name}_map,")
        c_lines.append("};")
        c_lines.append("")

    (assets_dir / f"{stem}.c").write_text("\n".join(c_lines), encoding="utf-8")
    print(f"Generated {len(icons)} ARGB8888 icons in {assets_dir / (stem + '.c')}")


def main() -> None:
    write_assets(
        "db_icons",
        [(name, SIZE, draw) for name, draw in ICONS],
        "Auto-generated Warm Cream pastel icons for Desk Buddy (tools/gen_icons.py)",
    )


if __name__ == "__main__":
    main()
