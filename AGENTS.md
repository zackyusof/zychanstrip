# AGENTS.md — zychanstrip (rackstrip)

Project-level instructions for any agent working in this repository.
The top-level `README.md` is the handover document; read it first.

## What this is

A JUCE 8 Audio Unit (`aufx`/`Zych`/`ZYUS`) presenting four classic outboard modules in a
re-orderable rack, plus a metering page. All DSP lives in `Source/Modules.h` (with the
EQ core in `Source/Eq560.h`); there is no other audio path.

## Ground rules

1. **Never change an existing parameter ID, range or default.** Append new parameters at
   the end of `createLayout()` and add them to the `setStateInformation` migration list.
2. **The rack order is a permutation of four module ids** packed into one `uint32_t`
   (`Console=0, Opto=1, Fet=2, Limiter=3`; default `[0,2,1,3]`). Any change to ordering
   must keep `setOrder()`'s permutation check and the packed-atomic read.
3. **Latency is reported once, from `prepareToPlay()`**, and must stay constant across
   every order and bypass combination.
4. **The metering tap is read-only** and sits after master gain and dry/wet mix.
5. Build and test on macOS only (`cmake`/`ninja`, then `tools/build_snapshot.py` →
   `RESULT 216/216`). The Linux container can compile the DSP research harness in
   `tools/research/` but cannot build the AU.
6. Commit and push every change (`main` on `https://git.zyusof.net/zack/rackstrip`).
   Keep the harness green before pushing; record what you could not verify.

## Documentation duties

* DSP behaviour changes → update `docs/dsp/` (module page + interaction/reorder pages).
* New open defect → add a row to the "Known issues" table in `README.md` with an ID.
* Specs for feature work live in `docs/specs/<slug>.md`.

## Where things are

| Concern | File |
| --- | --- |
| Module DSP, colour functions, limiter | `Source/Modules.h` |
| EQ560 core (MIT, adapted) | `Source/Eq560.h` |
| Processor, parameters, order packing, mix, state | `Source/PluginProcessor.{h,cpp}` |
| Metering measurement | `Source/Meters.h` |
| Meter page | `Source/MeterPanel.{h,cpp}` |
| Rack UI / LookAndFeel | `Source/PluginEditor.{h,cpp}` |
| Headless test + snapshot harness | `tools/snapshot_main.cpp`, `tools/dsp_checks.h` |
| DSP research harness (Linux-capable) | `tools/research/` |
