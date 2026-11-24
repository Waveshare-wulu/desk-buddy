#!/usr/bin/env python3
"""
Package Desk Buddy ESP-IDF build outputs for Waveshare Hub (https://hub.waveshare.net/):
- Copies split flash binaries (bootloader, partition-table, app) with exact offsets
- Generates a single merged flashable binary (0x0) via esptool merge-bin
- Emits a SHA-256 manifest, Windows flash helpers, and the font license
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build", help="ESP-IDF build directory")
    parser.add_argument("--out-dir", default="release", help="Output release directory")
    parser.add_argument("--version", default="v1.0.0", help="Firmware version tag")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    build_dir = (root / args.build_dir).resolve()
    out_dir = (root / args.out_dir).resolve()
    flasher_json = build_dir / "flasher_args.json"
    if not flasher_json.exists():
        print(f"ERROR: {flasher_json} not found. Run 'idf.py build' first.", file=sys.stderr)
        return 1

    data = json.loads(flasher_json.read_text(encoding="utf-8"))
    flash_files: dict[str, str] = data["flash_files"]
    write_args: list[str] = data.get("write_flash_args", [])
    chip: str = data.get("extra_esptool_args", {}).get("chip", "esp32s3")

    if out_dir.exists():
        shutil.rmtree(out_dir)
    bin_dir = out_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    ordered = sorted(flash_files.items(), key=lambda kv: int(kv[0], 0))
    manifest_files = []
    merge_pairs: list[str] = []

    for offset, rel_src in ordered:
        src = (build_dir / rel_src).resolve()
        dst_name = f"{offset}_{src.name}"
        dst = bin_dir / dst_name
        shutil.copy2(src, dst)
        manifest_files.append({
            "offset": offset,
            "file": f"bin/{dst_name}",
            "size_bytes": dst.stat().st_size,
            "sha256": sha256_file(dst),
        })
        merge_pairs.extend([offset, str(dst)])

    merged_name = f"desk-buddy-{args.version}-esp32-s3-touch-amoled-2.16-merged.bin"
    merged_path = out_dir / merged_name

    esptool_cmd = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        chip,
        "merge-bin",
        "-o",
        str(merged_path),
        *write_args,
        *merge_pairs,
    ]
    res = subprocess.run(esptool_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr, file=sys.stderr)
        return res.returncode

    manifest = {
        "project": "Desk Buddy (摸鱼搭子)",
        "version": args.version,
        "board": "Waveshare ESP32-S3-Touch-AMOLED-2.16",
        "chip": chip,
        "flash_size": data.get("flash_settings", {}).get("flash_size", "16MB"),
        "flash_mode": data.get("flash_settings", {}).get("flash_mode", "qio"),
        "flash_freq": data.get("flash_settings", {}).get("flash_freq", "80m"),
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "merged_firmware": {
            "offset": "0x0",
            "file": merged_name,
            "size_bytes": merged_path.stat().st_size,
            "sha256": sha256_file(merged_path),
        },
        "split_files": manifest_files,
        "notes": "The merged image at 0x0 overwrites NVS. To retain Desk Buddy settings and saved data, flash the split files at their listed offsets with Erase Flash disabled.",
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    shutil.copy2(root / "docs" / "NotoSansCJK-OFL.txt", out_dir)

    split_cmd_win = " ".join(f"{f['offset']} {f['file']}" for f in manifest_files)
    (out_dir / "flash_merged.bat").write_text(
        f"@echo off\n"
        f"set PORT=%1\n"
        f"if \"%PORT%\"==\"\" (echo Usage: flash_merged.bat COMx & exit /b 2)\n"
        f"cd /d %~dp0\n"
        f"python -m esptool --chip {chip} --port %PORT% --baud 460800 write-flash 0x0 {merged_name}\n",
        encoding="utf-8",
    )
    (out_dir / "flash_split.bat").write_text(
        f"@echo off\n"
        f"set PORT=%1\n"
        f"if \"%PORT%\"==\"\" (echo Usage: flash_split.bat COMx & exit /b 2)\n"
        f"cd /d %~dp0\n"
        f"python -m esptool --chip {chip} --port %PORT% --baud 460800 write-flash {' '.join(write_args)} {split_cmd_win}\n",
        encoding="utf-8",
    )
    print(f"Packaged release into {out_dir}")
    print(f"Merged binary: {merged_path} ({merged_path.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
