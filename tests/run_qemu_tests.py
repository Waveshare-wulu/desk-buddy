#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True, help="Path to qemu-system-xtensa executable")
    parser.add_argument("--flash-bin", required=True, help="Path to 4MB merged flash image")
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    cmd = [
        args.qemu,
        "-nographic",
        "-machine",
        "esp32s3",
        "-drive",
        f"file={args.flash_bin},if=mtd,format=raw",
        "-no-reboot",
    ]
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )

    exit_code = 1
    deadline = time.monotonic() + args.timeout
    try:
        assert proc.stdout is not None
        while time.monotonic() < deadline:
            line = proc.stdout.readline()
            if not line:
                if proc.poll() is not None:
                    break
                time.sleep(0.05)
                continue
            print(line, end="")
            if "TEST_RUNNER_EXIT_CODE=" in line:
                val = line.split("TEST_RUNNER_EXIT_CODE=", 1)[1].strip()
                exit_code = int(val)
                break
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
