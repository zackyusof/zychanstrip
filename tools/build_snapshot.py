#!/usr/bin/env python3
"""Build a headless GUI snapshot tool reusing the exact flags Ninja uses for the plugin.

Renders the RackStrip editor to snapshot.png so the GUI can be reviewed without a DAW
(and without a working AudioComponentRegistrar).
"""
import argparse
import os
import re
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", default=None, help="Configured Ninja build directory")
args = parser.parse_args()
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.abspath(os.path.expanduser(args.build_dir or os.path.join(REPO, "build")))
OUT = os.path.join(BUILD, "snapshot")
os.makedirs(OUT, exist_ok=True)

ninja = open(os.path.join(BUILD, "build.ninja")).read().split("\n")
start = next(i for i, l in enumerate(ninja) if "Source/PluginEditor.cpp.o:" in l)
block = ninja[start:start + 12]
vars_ = {}
for line in block:
    m = re.match(r"\s+(DEFINES|FLAGS|INCLUDES) = (.*)", line)
    if m:
        vars_[m.group(1)] = m.group(2)

defines = [d for d in shlex.split(vars_["DEFINES"]) if "STANDALONE" not in d]
includes = shlex.split(vars_["INCLUDES"]) + ["-I" + os.path.join(REPO, "Source")]
flags = [f for f in shlex.split(vars_["FLAGS"]) if not f.startswith("-flto") and not f.startswith("-W")]

sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
cxx = subprocess.check_output(["xcrun", "-f", "clang++"], text=True).strip()

src = os.path.join(REPO, "tools", "snapshot_main.cpp")
obj = os.path.join(OUT, "snapshot_main.o")

cmd = [cxx] + flags + defines + includes + ["-isysroot", sdk, "-c", src, "-o", obj]
print("compile:", " ".join(cmd[:6]), "...")
subprocess.check_call(cmd)

target_name = "zychanstrip" if os.path.exists(os.path.join(BUILD, "CMakeFiles", "zychanstrip.dir")) else "RackStrip"
objs = []
for root, _dirs, files in os.walk(os.path.join(BUILD, "CMakeFiles", f"{target_name}.dir")):
    for f in files:
        if not f.endswith(".o"):
            continue
        if "juce_audio_plugin_client" in root or f.startswith("juce_audio_plugin_client"):
            continue
        objs.append(os.path.join(root, f))

fw = ["Cocoa", "CoreAudio", "CoreAudioKit", "CoreMIDI", "AudioToolbox", "CoreFoundation", "IOKit", "Foundation",
      "AppKit", "QuartzCore", "CoreServices", "Metal", "MetalKit", "WebKit", "Security",
      "Accelerate", "CoreVideo", "DiscRecording", "AVFoundation", "CoreMedia", "AudioUnit",
      "Carbon", "UniformTypeIdentifiers"]
link = [cxx, "-arch", "arm64", "-isysroot", sdk, "-o", os.path.join(OUT, "snapshot"), obj] + objs
for f in fw:
    link += ["-framework", f]
link += ["-lz", "-lbz2", "-liconv"]
print("link:", len(objs), "objects")
subprocess.check_call(link)
print("built", os.path.join(OUT, "snapshot"))
