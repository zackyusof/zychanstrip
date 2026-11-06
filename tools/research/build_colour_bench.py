#!/usr/bin/env python3
"""Build the Colour Engine CPU bench against the current release objects.

The bench links `Source/ColourEngine.cpp` from the plugin's own build directory plus the
NAM objects, so it measures the exact code the AU ships. It is deliberately *not* part of
the plugin target: it needs a configured `build/` (macOS).

    python3 tools/research/build_colour_bench.py     # -> build/colour_bench
    ./build/colour_bench
"""
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.path.join(REPO, "build")
OBJ = os.path.join(BUILD, "CMakeFiles", "zychanstrip.dir")
OUT = os.path.join(BUILD, "colour_bench")


def objects():
    wanted = [os.path.join(OBJ, "Source", "ColourEngine.cpp.o")]
    for sub in ("", "wavenet"):
        d = os.path.join(OBJ, "_deps", "namcore-src", "NAM", sub)
        if os.path.isdir(d):
            wanted += sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".o"))
    missing = [p for p in wanted if not os.path.isfile(p)]
    if missing:
        sys.exit("missing objects (configure and build the plugin first):\n  " + "\n  ".join(missing))
    return wanted


def main():
    if not os.path.isdir(OBJ):
        sys.exit(f"no plugin build at {BUILD}; run cmake -B build -G Ninja first")
    cmd = [
        "c++", "-std=c++20", "-O3", "-DNDEBUG",
        "-I", os.path.join(REPO, "Source"),
        "-I", os.path.join(BUILD, "nam-assets"),
        "-I", os.path.join(BUILD, "_deps", "namcore-src"),
        os.path.join(REPO, "tools", "research", "colour_bench.cpp"),
        *objects(),
        "-framework", "Accelerate",
        "-o", OUT,
    ]
    subprocess.run(cmd, check=True)
    print(f"built {OUT}\nrun it with: {OUT}")


if __name__ == "__main__":
    main()
