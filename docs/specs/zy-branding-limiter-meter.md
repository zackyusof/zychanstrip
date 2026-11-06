# SPEC — zy faceplate branding and two-row limiter meters

| Field | Value |
| --- | --- |
| Slug | `zy-branding-limiter-meter` |
| Repo | `~/Documents/Codex/zychanstrip` (Gitea `zack/zychanstrip`) |
| Track | GUI presentation |
| Owner | zack |
| Created | 2026-11-06 |
| Classification | feature (GUI presentation only) |
| Version authority | **user** (the agent never invents a version, tag or release) |
| Requested base | `9baea2a` (main == origin/main at start) |

## Phase status

| # | Phase | Status | Artifact | Evidence |
| --- | --- | --- | --- | --- |
| 1 | Requirements | done | §1 | user brief 2026-11-06 + mock `/Users/music/Desktop/mock.jpeg` |
| 2 | Design | done | §2 | DES-1…DES-5 |
| 3 | Build | done | §3 | `ninja -C build` clean; snapshot harness built |
| 4 | Test | done | §4 | `RESULT 593/593 passed`; rack snapshots at 904x686, 1128x856, 1692x1284 |
| 5 | Deploy | done | §5 | `tools/install_global.sh` (global AU only) |
| 6 | Maintain | done | §6 | changelog, README, memory entry |

## 1. Requirements

1. Keep the approved rack layout of the mock: same four faceplates, header, wood frame.
2. Branding becomes the zy product line: `zy-560`, `zy-76`, `zy-la`, `zy-limiter`.
3. Limiter (“enhancer”) GR meter: **two rows of ten**, a stereo left/right indicator, with the
   dB numbering printed only once under the bottom row, thinner housing, and the housing
   exactly covering the THRESHOLD / CEILING caption span.
4. Limiter OUT meter: extended **upward** so its top edge aligns with the GR meter height.
5. Build and install a new beta (same numeric line, next prerelease).

Scope: `Source/PluginEditor.cpp` only (drawing + layout). No DSP, parameter IDs, ranges,
defaults, rack ordering, latency or state migration changes.

## 2. Design

| ID | Decision |
| --- | --- |
| DES-1 | Console wordmark `560` → `zy-560` (light blue, top right; box widened to 96 px at 21 pt so it does not clip). |
| DES-2 | FET wordmark `urei` → `zy-76`; sub-plate `MODEL 1176 LN` → `MODEL zy-76`; model badge `1176` → `zy-76`. |
| DES-3 | Opto wordmark `TELETRONIX` → `zy-la` in the sub-plate, on the round meter face and in the model badge. |
| DES-4 | Limiter gains a `zy-limiter` wordmark centred between the AUTO GAIN and BYPASS headers. |
| DES-5 | GR housing `{c.x, top+2, 2·cellW, 42}` — exactly the two caption cell widths, thin. Two rows of ten LEDs (top = left channel, bottom = right channel) on a 1, 2, 3, 4, 6, 8, 10, 12, 18, 24 dB scale; numbers once under the bottom row. OUT ladder `{right-30, GR.y, 30, bankBottom-GR.y}`, 32 steps −48…0 dBFS. gridTop raised to `top+62` so the 2x2 fader bank grows into the freed space. |

## 3. Build

```
python3 tools/nam/prepare_profiles.py build/nam-assets
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZYCHANSTRIP_BETA=beta.9
ninja -C build
python3 tools/build_snapshot.py
```

## 4. Test

Headless harness (`build/snapshot/snapshot <dir>`) must report `RESULT <total>/<total> passed`;
`rack-1128x856.png` rendered and compared against the mock for branding and meter geometry; `docs/screenshots/zychanstrip-rack.png` refreshed from that render.

## 5. Deploy

`tools/install_global.sh` writes `/Library/Audio/Plug-Ins/Components/zychanstrip.component`
only (never user space) and refuses a dirty tree or a stale artefact.

## 6. Maintain

CHANGELOG + README beta references updated; open defects recorded in README's Known issues.
