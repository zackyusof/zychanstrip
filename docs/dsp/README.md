# zychanstrip — DSP reference

Deep documentation of what every module actually does to the signal, how the modules
interact along the rack path, and what happens when the rack is re-ordered.

This folder complements the top-level `README.md` (which is the quick-start / build /
handover guide). Everything here is derived from the shipped source at the commit noted
in each file, with `file:line` citations so a claim can be re-checked.

| Document | Scope |
| --- | --- |
| [`signal-path.md`](signal-path.md) | End-to-end flow: buffer ownership, order packing, latency, master, mix, metering tap |
| [`module-console.md`](module-console.md) | `ConsoleModule` — API 560 ten-band graphic EQ + 2520 colour |
| [`module-opto.md`](module-opto.md) | `OptoModule` — LA-2A style optical leveller + tube colour |
| [`module-fet.md`](module-fet.md) | `FetModule` — 1176 style FET compressor + Class-A colour |
| [`module-limiter.md`](module-limiter.md) | `LimiterModule` — look-ahead brickwall limiter, sliding-minimum CPU optimization, Enhance, auto-gain |
| [`interaction.md`](interaction.md) | How modules interact: level, envelope, colour stacking, auto-gain feedback |
| [`reordering.md`](reordering.md) | What re-ordering changes (and what it provably does not), per permutation |
| [`measurements.md`](measurements.md) | Measured numbers from `tools/research/permute.cpp` on this repo |
| [`research/`](research/) | Raw lane notes, kept for provenance (superseded by the files above) |

## Reading order

1. `signal-path.md` — the frame everything else lives in.
2. The four `module-*.md` files — one per rack module.
3. `interaction.md` — the pairwise and global couplings.
4. `reordering.md` + `measurements.md` — the answer to "what happens if I drag this module?"

## Evidence convention

Every quantitative statement is either

* **derived** — a formula quoted with the source line that implements it, or
* **measured** — produced by `tools/research/permute.cpp` (a headless, JUCE-free
  re-implementation of the exact DSP in `Source/Modules.h`) and recorded in
  `measurements.md`.

Where a claim is reasoning rather than measurement it is marked *(inference)*.
Nothing in this folder is copied from marketing copy or from the hardware manuals.

- [Beta.2 NAM colour engines, resampling, calibration, latency and validation](nam-colour.md)
