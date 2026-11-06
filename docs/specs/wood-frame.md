# Wooden GUI frame

## Scope

Add the approved figured-wood frame around the existing live rack and meter pages. The mockup is
used only as a source for four wood-only texture strips; none of its older GUI is included. All
controls, DSP, and page interactions remain part of the current editor.

## Geometry and resize behavior

- The top and bottom rails are 48 logical pixels; the left and right rails are 24 logical pixels.
- The rack canvas and meter page use one shared scale and centered transform inside the frame.
- The outer resize ratio includes the rails, keeping the live 1080×760 canvas undistorted.
- Minimum, default, and maximum snapshots cover both pages; geometry checks assert matching bounds
  and the 2:1 top/bottom-to-side rail ratio.

## Assets

`Assets/wood_top.png`, `wood_bottom.png`, `wood_left.png`, and `wood_right.png` are wood-only crops
from the approved `zychanstrip-wood-frame-thin-sides.png` reference. JUCE embeds these assets into
the AU. `tools/build_snapshot.py` also links the generated data objects so snapshots exercise the
same frame resources as the plugin.
