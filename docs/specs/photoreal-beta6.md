# Faithful photographic rack — 1.2.0-beta.6

## Correction

Beta.5 retained procedural hardware and only substituted texture swatches. The user explicitly requested the complete approved render, including its slim machined 1176 knobs. This supersedes the earlier chrome-knob preference.

## Rendering

`Assets/reference/photoreal-rack-static-beta6.png` is an imagegen edit of the approved beta.5 reference: the same panels, knob bodies, glass, markings and reflections, with fader caps, knob pointers, VU needles and illuminated lamp states removed. It is embedded once by BinaryData. Module-local source rectangles let each complete photographic panel reorder independently.

All ten EQ caps, four limiter caps, six rotary indices, both VU needles, both meter ladders, ratio selection and switch lamps remain live. Hit targets and drawing coordinates share the reference image coordinate system. Host automation, parameter IDs/ranges/defaults, DSP and serialized state remain unchanged. Needles use the existing VU deflection law. Limiter LED thresholds remain 1/2/3/4/6/8/10/12/18/24 dB; their legible live scale replaces the generated artwork's inaccurate GR numerals. Readouts and the version label cover the reference's fixed values. The existing metering page stays live.

The reference has different upper/lower limiter track lengths; the layout follows those lengths. Fader repaint bounds include full caps/shadows at travel endpoints. The fixed photographed knob lighting does not rotate with its indicator.

## Verification

The snapshot harness adds host-automation checks for every rotary control and limiter fader, requiring visible motion within the control and no changed pixels on the surrounding faceplate. Existing resizing, drag reorder, page navigation, state and DSP checks remain. `snapshot <absolute-output-dir> --gui-only` is available for visual iteration; final validation uses the full suite and packaged AU probe.

Image preparation used the built-in imagegen tool. Prompt: preserve the approved image exactly, removing only dynamic EQ/limiter caps, VU needles, knob indices and LED illumination, and set the 4:1 key to its unselected state. The committed result makes runtime builds independent of image generation.
