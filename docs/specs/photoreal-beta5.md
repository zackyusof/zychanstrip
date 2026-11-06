# Photoreal skin — 1.2.0-beta.5

The user-approved generated rack image is stored in `Assets/reference/photoreal-rack-beta5.png`. `python3 tools/prepare_photo_skin.py` (Pillow) reproduces the wood-only rails, unlabelled metal swatches, and tightly cropped EQ/limiter caps used by the existing BinaryData asset target. The complete reference is not embedded into the plugin.

Keep the numeric version at 1.2.0; configure `ZYCHANSTRIP_BETA=beta.5`. Reduce the faceplate tint overlay so the photographic brushing remains visible. Add subtle fixed softbox highlights to both VU glasses above their moving needles. Preserve existing chrome knobs, LA-2A control geometry, all labels, automation, hit targets, resizing and reorder behavior. No DSP changes.

Verification: rebuild the macOS AU and snapshot harness, run all existing DSP/GUI checks, inspect rack snapshots at supported sizes, and run the packaged AU factory/render probe. Install the committed build with `tools/install_global.sh` using the configured build/xcode bundle path. DAW validation remains with the maintainer.
