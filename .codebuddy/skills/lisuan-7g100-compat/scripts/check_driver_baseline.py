#!/usr/bin/env python3
"""检查砺算 7G100 厂商驱动是否符合项目要求的 v262.2.0.56 基准。

该脚本只读，不修改系统。保持 BASELINE 与
packaging/lisuan_fix_core.py::REQUIRED_LISUAN_DRIVER_VERSION 同步。
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

BASELINE = "262.2.0.56"
PACKAGES = ("lisuangpu-dkms", "lisuangpu-lib")


def query_version(package: str) -> str | None:
    try:
        result = subprocess.run(
            ["dpkg-query", "-W", "-f=${Version}", package],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def matches_baseline(version: str) -> bool:
    candidate = version.strip().rsplit(":", 1)[-1]
    if candidate == BASELINE:
        return True
    return any(candidate.startswith(BASELINE + sep)
               for sep in ("-", "+", "~", "."))


def collect() -> dict:
    packages = {}
    for package in PACKAGES:
        version = query_version(package)
        packages[package] = {
            "installed": version is not None,
            "version": version,
            "matches_baseline": matches_baseline(version) if version else False,
        }

    source_dirs = sorted(
        str(path) for path in Path("/usr/src").glob(f"lisuangpu-dkms-{BASELINE}*")
        if path.is_dir()
    )
    return {
        "required_driver_version": f"v{BASELINE}",
        "packages": packages,
        "dkms_source_dirs": source_dirs,
        "ok": all(item["matches_baseline"] for item in packages.values()),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="输出 JSON")
    args = parser.parse_args()
    report = collect()

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"要求驱动版本：v{BASELINE}")
        for package, item in report["packages"].items():
            if not item["installed"]:
                print(f"[缺失] {package}")
            elif item["matches_baseline"]:
                print(f"[通过] {package} {item['version']}")
            else:
                print(f"[不匹配] {package} {item['version']}")
        if report["dkms_source_dirs"]:
            print("DKMS 源码目录：")
            for path in report["dkms_source_dirs"]:
                print(f"  {path}")
        print("结论：" + ("符合验证基准" if report["ok"] else "不符合验证基准"))

    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
