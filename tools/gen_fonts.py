#!/usr/bin/env python3
"""
Generate 4 bpp LVGL font subsets for the characters in Desk Buddy source files.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SIZES = [16, 20, 26, 36]
HERO_SIZE = 48
HERO_RANGE = "0x21,0x2D,0x30-0x3A"

ALWAYS_INCLUDED_CHARS = (
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
    "，。！？、：；“”‘’（）【】《》—…·～℃%+-×÷=★☆●○◆◇▲▼←→↑↓‹›"
    "一二三四五六七八九十百千万亿零年月日时分秒周星期天地上下左右中大小"
    "开关确认取消返回重置保存开始暂停继续完成设置音量亮度休眠熄屏电池充电"
    "摇一摇点击随机今日运势宜忌摸鱼搭子功德木鱼亲戚称呼计算器番茄钟下班倒计时"
)


def read_ttf_cmap(font_path: Path) -> set[int]:
    import struct
    data = font_path.read_bytes()
    num_tables = struct.unpack_from(">H", data, 4)[0]
    cmap_off = None
    for i in range(num_tables):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12 + i * 16)
        if tag == b"cmap":
            cmap_off = offset
            break
    if cmap_off is None:
        raise ValueError(f"No cmap table found in {font_path}")

    num_sub = struct.unpack_from(">H", data, cmap_off + 2)[0]
    supported: set[int] = set()
    for i in range(num_sub):
        plat, enc, rel = struct.unpack_from(">HHI", data, cmap_off + 4 + i * 8)
        sub_off = cmap_off + rel
        fmt = struct.unpack_from(">H", data, sub_off)[0]
        if fmt == 4:
            seg_count = struct.unpack_from(">H", data, sub_off + 6)[0] // 2
            end_off = sub_off + 14
            start_off = end_off + seg_count * 2 + 2
            for s in range(seg_count):
                end_c = struct.unpack_from(">H", data, end_off + s * 2)[0]
                start_c = struct.unpack_from(">H", data, start_off + s * 2)[0]
                if start_c <= end_c and end_c != 0xFFFF:
                    supported.update(range(start_c, end_c + 1))
        elif fmt == 12:
            n_groups = struct.unpack_from(">I", data, sub_off + 12)[0]
            for g in range(n_groups):
                sc, ec, _ = struct.unpack_from(">III", data, sub_off + 16 + g * 12)
                if sc <= ec and ec <= 0xFFFF:
                    supported.update(range(sc, ec + 1))
    return supported


def collect_codepoints(root: Path) -> str:
    chars: set[str] = set(ALWAYS_INCLUDED_CHARS)
    scan_dirs = [root / "main", root / "components" / "desk_buddy_core"]
    exts = {".c", ".cpp", ".h", ".hpp"}

    for d in scan_dirs:
        if not d.exists():
            continue
        for f in d.rglob("*"):
            if f.suffix.lower() not in exts:
                continue
            if "assets" in f.parts:
                continue
            text = f.read_text(encoding="utf-8", errors="ignore")
            for ch in text:
                cp = ord(ch)
                if 0x20 <= cp <= 0x7E or cp >= 0xA0:
                    if cp not in (0xFEFF,):
                        chars.add(ch)

    bmp_chars = sorted(c for c in chars if (0x20 <= ord(c) <= 0xFFFF) and not (0xD800 <= ord(c) <= 0xDFFF))
    return "".join(bmp_chars)


def format_ranges(chars: str) -> str:
    cps = sorted({ord(c) for c in chars})
    ranges: list[str] = []
    start = prev = cps[0]
    for cp in cps[1:]:
        if cp == prev + 1:
            prev = cp
        else:
            ranges.append(f"0x{start:04X}-0x{prev:04X}" if start != prev else f"0x{start:04X}")
            start = prev = cp
    ranges.append(f"0x{start:04X}-0x{prev:04X}" if start != prev else f"0x{start:04X}")
    return ",".join(ranges)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regular", required=True, type=Path, help="Static Noto Sans CJK SC Regular font")
    parser.add_argument("--bold", required=True, type=Path, help="Static Noto Sans CJK SC Bold font")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    font_regular = args.regular.resolve(strict=True)
    font_bold = args.bold.resolve(strict=True)
    assets_dir = root / "main" / "ui" / "assets"
    assets_dir.mkdir(parents=True, exist_ok=True)

    supported_cps = read_ttf_cmap(font_regular) & read_ttf_cmap(font_bold)
    symbols = collect_codepoints(root)
    missing = [c for c in symbols if ord(c) not in supported_cps]
    if missing:
        parser.error(f"The input fonts lack {len(missing)} required glyph(s): {''.join(missing)}")
    range_arg = format_ranges(symbols)
    print(f"Font sources: regular={font_regular.name}, bold={font_bold.name}")
    print(f"Collected {len(symbols)} supported glyphs ({len(range_arg.split(','))} ranges) from Desk Buddy sources.")

    npx = shutil.which("npx") or shutil.which("npx.cmd")
    if not npx:
        raise RuntimeError("npx not found in PATH")

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

    outputs = [(f"db_font_{sz}", sz, font_regular if sz <= 16 else font_bold, range_arg) for sz in SIZES]
    outputs.append((f"db_font_hero_{HERO_SIZE}", HERO_SIZE, font_bold, HERO_RANGE))
    for name, sz, src_font, glyphs in outputs:
        out_file = assets_dir / f"{name}.c"
        cmd = [
            npx,
            "--yes",
            "lv_font_conv@1.5.3",
            "--font",
            src_font.name,
            "--size",
            str(sz),
            "--bpp",
            "4",
            "--no-compress",
            "--format",
            "lvgl",
            "--lv-include",
            "lvgl.h",
            "--range",
            glyphs,
            "-o",
            str(out_file),
        ]
        print(f"Generating {name}.c ...")
        res = subprocess.run(cmd, cwd=src_font.parent, capture_output=True, text=True,
                             encoding="utf-8", errors="replace")
        if res.returncode != 0:
            print(res.stdout)
            print(res.stderr, file=sys.stderr)
            raise RuntimeError(f"lv_font_conv failed for {name}")
        generated = out_file.read_text(encoding="utf-8")
        generated = generated.replace(f" -o {out_file}\n", f" -o {out_file.name}\n", 1)
        out_file.write_text(generated, encoding="utf-8")
        header_lines.append(f"LV_FONT_DECLARE({name});")

    header_lines.extend(["", "#ifdef __cplusplus", "}", "#endif", ""])
    (assets_dir / "db_fonts.h").write_text("\n".join(header_lines), encoding="utf-8")
    print(f"Generated db_fonts.h and {len(outputs)} font files successfully.")


if __name__ == "__main__":
    main()
