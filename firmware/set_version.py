#!/usr/bin/env python3
"""
Pre-build script: reads project version from version.txt and injects APP_VERSION_STR
as a compile-time define so firmware has a single version source of truth.
"""
Import("env")
from pathlib import Path

project_dir = Path(env["PROJECT_DIR"])
version_file = project_dir / "version.txt"
version = "0.0.0"

if version_file.exists():
    text = version_file.read_text(encoding="utf-8").strip()
    if text:
        version = text

# Inject as a quoted C string macro, e.g. -DAPP_VERSION_STR="1.4.2"
env.Append(BUILD_FLAGS=[f'-DAPP_VERSION_STR=\\"{version}\\"'])
print(f"Using APP_VERSION_STR={version}")
