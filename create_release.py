#!/usr/bin/env python3
"""Create VUURWERK v1.0.0 verified release package"""

import zipfile
import os
from pathlib import Path

def create_release_zip():
    output_file = "../vuurwerk-v1.0.0-20260213-VERIFIED.zip"

    # Files to include
    files = [
        "vuurwerk-v1.0.0.bin",
        "ALGORITHMS.md",
        "BUILD.md",
        "CHANGELOG.md",
        "FLASHING.md",
        "RELEASE_NOTES.md",
        "README.md",
        "size_report.txt",
        "Makefile",
    ]

    # Source directories to include (full recursion)
    source_dirs = [
        "app", "bsp", "driver", "helper", "ui"
    ]

    # Individual source files in root
    root_sources = ["syscalls.c", "main.c", "version.c"]

    # Header files and linker scripts in root
    for f in os.listdir("."):
        if f.endswith((".h", ".ld")):
            root_sources.append(f)

    print(f"Creating {output_file}...")

    with zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED) as zf:
        # Add individual files
        for f in files + root_sources:
            if os.path.exists(f):
                zf.write(f, f"vuurwerk-v1.0.0/{f}")
                print(f"  + {f}")

        # Add main source directories recursively
        for dir_name in source_dirs:
            if os.path.isdir(dir_name):
                for root, dirs, files in os.walk(dir_name):
                    for file in files:
                        # Skip build artifacts
                        if file.endswith(('.o', '.d', '.elf', '.map', '.lst')):
                            continue

                        file_path = os.path.join(root, file)
                        arcname = f"vuurwerk-v1.0.0/{file_path}"
                        zf.write(file_path, arcname)

                print(f"  + {dir_name}/ (all sources)")

        # Add only essential CMSIS_5 files (not the entire 51MB library)
        cmsis_essential = [
            "external/printf",  # Full printf library (small)
            "external/CMSIS_5/CMSIS/Core/Include",  # Core headers
            "external/CMSIS_5/Device/ARM/ARMCM0/Include",  # Device headers
        ]

        for path in cmsis_essential:
            if os.path.exists(path):
                if os.path.isfile(path):
                    zf.write(path, f"vuurwerk-v1.0.0/{path}")
                else:
                    for root, dirs, files in os.walk(path):
                        for file in files:
                            file_path = os.path.join(root, file)
                            arcname = f"vuurwerk-v1.0.0/{file_path}"
                            zf.write(file_path, arcname)
                print(f"  + {path}")

    # Get final size
    size_mb = os.path.getsize(output_file) / (1024 * 1024)
    print(f"\nRelease package created: {output_file}")
    print(f"Size: {size_mb:.2f} MB")

    return output_file

if __name__ == "__main__":
    create_release_zip()
