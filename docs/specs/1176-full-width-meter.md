# 1176 full-width VU meter

The 1176 VU housing spans the full module faceplate width, removing the fixed
side margins. Keep its vertical position, height, dial artwork, needle behaviour
and surrounding controls unchanged. The editor's existing canvas transform scales
the meter with the rack at every supported window size.

Deliver only the macOS arm64 Audio Unit, install the single system-wide copy,
run the snapshot harness and inspect the rendered rack at all three sizes.
Commit and push the source and upload the AU archive to the existing Gitea release.
Keep version 1.0.1.

Verified on macOS (2026-11-01): AU Release build passed; snapshot harness
246/246 before and after; rack snapshots inspected at 864×608, 1080×760 and
1620×1140. System bundle is root:wheel, codesign verification passed, component
identity is aufx/Zych/ZYUS at version 65537. Direct factory Open/Close returned
0/0 (construction probe with a null host instance). User-level duplicate removed.
Host validation remains with the user, as required by the repository policy.
