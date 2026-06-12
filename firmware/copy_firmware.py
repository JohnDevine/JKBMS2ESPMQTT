#!/usr/bin/env python3
"""
Post-build script: copies firmware.bin, generates spiffs.bin via spiffsgen.py
"""
Import("env")
import shutil
import subprocess
import os
from pathlib import Path

def copy_firmware_files(source, target, env):
    print("=" * 50)
    print("Copying firmware files to firmware/ folder...")
    
    project_dir = Path(env["PROJECT_DIR"])
    build_dir = Path(project_dir) / ".pio" / "build" / env["PIOENV"]
    firmware_dir = project_dir / "firmware"
    firmware_dir.mkdir(exist_ok=True)

    # Copy firmware.bin
    fw = build_dir / "firmware.bin"
    if fw.exists():
        shutil.copy2(fw, firmware_dir / "firmware.bin")
        size = (firmware_dir / "firmware.bin").stat().st_size
        print(f"firmware.bin -> firmware/ ({size:,} bytes)")
    
    # Generate spiffs.bin using spiffsgen.py (handles aarch64 where mkspiffs doesn't run)
    data_dir = project_dir / "data"
    spiffs_out = build_dir / "spiffs.bin"
    spiffsgen = (Path(env["IDF_PATH"]) if "IDF_PATH" in env 
                 else Path(project_dir) / ".pio" / "packages" / "framework-espidf"
                ) / "components" / "spiffs" / "spiffsgen.py"
    
    # Try alternate IDF paths
    if not spiffsgen.exists():
        candidates = [
            Path(project_dir) / ".pio" / "packages" / "framework-espidf",
        ]
        home_pio = Path.home() / ".platformio" / "packages"
        if home_pio.exists():
            for d in home_pio.iterdir():
                if "framework-espidf" in d.name:
                    candidates.append(d)
        for c in candidates:
            p = c / "components" / "spiffs" / "spiffsgen.py"
            if p.exists():
                spiffsgen = p
                break

    if data_dir.exists() and any(data_dir.iterdir()):
        try:
            result = subprocess.run(
                ["python3", str(spiffsgen), "0x170000", str(data_dir), str(spiffs_out)],
                capture_output=True, text=True, timeout=30
            )
            if result.returncode == 0 and spiffs_out.exists():
                shutil.copy2(spiffs_out, firmware_dir / "spiffs.bin")
                size = (firmware_dir / "spiffs.bin").stat().st_size
                print(f"spiffs.bin -> firmware/ ({size:,} bytes)")
            else:
                print(f"spiffsgen.py failed: {result.stderr.strip()}")
        except Exception as e:
            print(f"spiffsgen.py error: {e}")
    else:
        print("data/ directory empty or missing, skipping spiffs.bin")

    print("Done")
    print("=" * 50)

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware_files)
