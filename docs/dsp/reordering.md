# Re-ordering — what actually changes when you drag a module

The rack is a permutation of four modules (`Console`, `Opto`, `Fet`, `Limiter`) stored in
one atomic word (`Source/PluginProcessor.h:58`). The UI reorders by drag; the audio thread
picks the new order up on the next block boundary. Nothing is re-prepared, nothing is
re-allocated, latency does not move.

That is the plumbing. This page is about the **sound**.

Numbers are from `tools/research/permute.cpp` (see [`measurements.md`](measurements.md)),
48 kHz, mastering preset with auto-gain off unless stated.

---

## 1. The one-sentence rule

> Re-ordering changes **what each detector sees** and **which non-linearity runs last**.
> Everything else — latency, master, mix, meters — is fixed after the rack.

Two consequences follow, and they explain almost every "it sounds different now" report:

1. **Detector drive.** Three of the four modules are level-dependent (both compressors and
   the limiter). Whatever runs first gets the uncompressed programme; whatever runs last
   gets the already-compressed programme. A 3 dB change of detector drive is worth more
   than any colour-stage difference.
2. **Output stage position.** Only the limiter guarantees a ceiling. Put a colour stage
   after it and the ceiling is no longer a ceiling.

---

## 2. Measured spread across all 24 orders

| preset | signal | RMS spread | peak spread |
| --- | --- | --- | --- |
| all controls neutral | multitone | 0.69 dB | 0.21 dB |
| all controls neutral | transients | 0.71 dB | 0.12 dB |
| mastering | multitone | 6.96 dB | 7.08 dB |
| mastering | transients | 6.12 dB | 6.40 dB |
| mastering + auto-gain | multitone | 4.25 dB | 5.76 dB |

Read that as: with a real mastering setting, **the order is worth about 7 dB of level**
and about 7 dB of peak. It is not a subtle effect, and it is not the colour stages doing
it (§6 below shows colour stacking is worth < 0.3 dB in the band bins).

Even the "neutral" rack spreads 0.69 dB, because `FetModule` has a **fixed −10 dBFS
threshold** (`Source/Modules.h:260`) — at input 0 dB it is already compressing 4–5 dB.
There is no "everything off" position for the FET.

---

## 3. Module by module: what moving it does

### Moving the Console (EQ560)

* **Before a compressor** (its natural place): the EQ shapes what the detector hears.
  A +3 dB boost at 1 kHz makes the FET and the opto compress the midrange harder; a low-end
  boost makes them duck the whole programme on every kick.
* **After a compressor**: the EQ shapes tone but no longer steers gain reduction. The
  compressor reacts to the un-equalised programme. This is the "EQ as a corrective
  after-stage" position.
* **After the limiter**: the ceiling is no longer enforced (measured: two orders with
  opto last break a −1.0 dB ceiling by 0.2–0.37 dB; the EQ's colour does the same kind of
  thing at high band gains).
* The EQ is the only module with its own **oversampling** and internal smoothing, so it
  is also the only module whose *transient* behaviour changes with position: ahead of a
  fast FET it can shave the attack before the detector sees it.

### Moving the Opto (LA-2A)

* **Before the FET**: the opto's slow, program-dependent levelling reduces the drive the
  FET detector sees (measured: FET gain reduction 12.8 dB → 11.2 dB when the opto moves in
  front of it, and the opto itself works harder: 2.2 → 4.9 dB).
* **After the FET**: the FET does the peak work, the opto smooths what is left. This is the
  classic 1176 → LA-2A order and the reason the default is `[0,2,1,3]`.
* **After the limiter**: the opto's tube colour adds 2nd-harmonic asymmetry *after* the
  ceiling clamp — the only measured ceiling breaches in the whole matrix come from this
  (`measurements.md` §3: −0.80 dB and −0.63 dB against a −1.0 dB ceiling).

### Moving the FET (1176)

* **First**: it sees the raw programme and does the most gain reduction (11.2 dB in the
  measured preset), and its Class-A colour is applied to un-compressed transients.
* **Last before the limiter**: it shapes what the limiter has to catch; the limiter's own
  gain reduction falls (9.9 → 7.7 dB when the limiter moves to the front instead).
* The FET's attack/release are extremely fast (0.02–0.8 ms attack, 50–1100 ms release,
  `Source/Modules.h:245-246`); its position therefore changes the *envelope* of the signal
  that follows, not just its level.

### Moving the Limiter (Enhance)

* **Last** (default): a real output ceiling, auto-gain compensating the whole upstream
  chain, and the Enhance saturation applied to the final programme.
* **First**: nothing downstream is protected. Every later module can push the peak back
  up. The auto-gain reference also becomes "input of the chain" — which sounds like the
  right thing but means it no longer matches the rack's output, so the mix control and the
  meters disagree with the correction.
* **Middle**: legal but pointless unless you are deliberately using a later compressor as
  a "post-limiter" colour stage — and then the ceiling is advisory only.

---

## 4. Order archetypes and what they are for

| order | character | when to use |
| --- | --- | --- |
| `EQ > FET > OPTO > LIM` (default) | 1176 catches peaks, LA-2A levels, limiter is the output stage | mastering and tracking where you want the classic stack |
| `EQ > OPTO > FET > LIM` | opto levels first, FET adds bite, limiter last | when the FET alone sounds too grabby |
| `FET > EQ > OPTO > LIM` | compressor before EQ, tone shaping post-dynamics | corrective EQ that must not steer the detector |
| `LIM > EQ > FET > OPTO` | limiter first = "safety" then tone | measured −8.29 dBFS vs −6.83 dBFS for the default: 1.5 dB quieter, and the ceiling is not enforced |
| any order with LIM not last | ceiling advisory | only for deliberate effect |

The default is not arbitrary: it is the only archetype where every downstream stage is
either linear-ish (EQ after? no — EQ is first) or the output limiter itself.

---

## 5. What to expect in the UI when you reorder

* **No glitch, no latency change.** The order is read once per block; the module objects
  keep their state. A compressor that was mid-release continues its release.
* **Meters move immediately** — they read post-chain, so a reorder that changes level shows
  up on the next block.
* **`grDb` meters are per module and stay with the module**, not the slot. Dragging the FET
  moves its gain-reduction meter with it, which makes before/after comparison easy.
* **Auto-gain may jump** by up to a couple of dB when the limiter's neighbours change
  (measured −1.89 dB vs −1.52 dB for limiter-last vs limiter-first).
* **The saved session keeps its order** — `order` is serialised with the state
  (`PluginProcessor.cpp:176-185`), so a reorder is a state change like any other.

---

## 6. What does *not* change (verified)

| Claim | Evidence |
| --- | --- |
| latency stays 71 samples at 48 kHz for every order | `measurements.md` §7; `PluginProcessor.cpp:100-106` |
| master gain, dry/wet, metering are post-rack | `PluginProcessor.cpp:132-149` |
| parameter values and bypass flags are order-independent | APVTS, not chain state |
| colour stacking is a second-order effect (< 0.3 dB band bins) | `measurements.md` §6 |
| detector drive is the first-order effect (~1.5 dB level per swap) | `measurements.md` §5 |

---

## 7. Practical checklist after changing the order

1. **Level-match before judging.** The same settings can differ by ~7 dB RMS; without
   matching you are judging loudness, not tone.
2. **Check the peak.** If any module is after the limiter, verify the true peak on the
   METER page — the ceiling is not enforced in that configuration.
3. **Watch both `grDb` meters.** If the FET is showing > 10 dB with the opto at 2 dB, the
   FET is doing all the work and the opto is decorative.
4. **Re-check auto-gain.** Its correction is relative to the limiter's own input; after a
   reorder it may hold a different reference point.
5. **Re-run the harness** (`tools/research/build.sh && ./build/permute`) if the DSP itself
   changed — the tables in `measurements.md` are only valid for the commit that produced
   them.
