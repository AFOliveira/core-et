#!/usr/bin/env python3
"""Run ACT4 ELFs on the Minion Verilator ACT simulator."""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
from pathlib import Path

SUMMARY_RE = re.compile(r'RVCP-SUMMARY: TEST (PASSED|FAILED) - Test File "([^"]+)"')


def run_one(sim: Path, elf: Path, max_cycles: int) -> tuple[bool, Path, str]:
    cmd = [str(sim), "--elf", str(elf), "--max-cycles", str(max_cycles)]
    result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    summaries = SUMMARY_RE.findall(result.stdout)
    ok = result.returncode == 0 and summaries and summaries[-1][0] == "PASSED"
    summary = summaries[-1][0] if summaries else "NO_SUMMARY"
    if not ok:
        log_dir = elf.parents[2] / "logs" / elf.parent.relative_to(elf.parents[1])
        log_dir.mkdir(parents=True, exist_ok=True)
        log_path = log_dir / f"{elf.stem}.log"
        log_path.write_text(result.stdout)
        return False, elf, f"{summary}: {log_path}"
    return True, elf, summary


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sim", type=Path, required=True)
    parser.add_argument("--elf-dir", type=Path, required=True)
    parser.add_argument("--extensions", default="")
    parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    parser.add_argument("--max-cycles", type=int, default=2_000_000)
    args = parser.parse_args()

    if not args.sim.exists():
        raise SystemExit(f"ACT simulator not found: {args.sim}")
    if not args.elf_dir.exists():
        raise SystemExit(f"ACT ELF directory not found: {args.elf_dir}")

    requested_extensions = {ext.strip() for ext in args.extensions.split(",") if ext.strip()}
    elfs = sorted(args.elf_dir.rglob("*.elf"))
    if requested_extensions:
        elfs = [elf for elf in elfs if elf.parent.name in requested_extensions]
    if not elfs:
        suffix = f" for extensions {','.join(sorted(requested_extensions))}" if requested_extensions else ""
        raise SystemExit(f"No ACT ELFs found under {args.elf_dir}{suffix}")

    failed: list[tuple[Path, str]] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_one, args.sim, elf, args.max_cycles) for elf in elfs]
        for future in concurrent.futures.as_completed(futures):
            ok, elf, detail = future.result()
            rel = elf.relative_to(args.elf_dir)
            if ok:
                print(f"  PASS {rel}")
            else:
                print(f"  FAIL {rel} - {detail}")
                failed.append((elf, detail))

    passed = len(elfs) - len(failed)
    print("")
    if failed:
        print(f"ACT RESULT: {len(failed)} failed, {passed} passed out of {len(elfs)} tests")
        return 1

    print(f"ACT RESULT: all {len(elfs)} tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
