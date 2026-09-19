# Password repair verification

The independently demonstrated P2 is repaired: `ViewerController::unlock`
converts its QString with `toLatin1()`, matching Poppler Qt6's documented
`Document::load` password contract. A concise boundary comment records why.
The reviewer's synthetic `latin1-password.pdf` (`café`) is preserved as binary
test data, with provenance/password in the fixture README. Its exact generator
is retained in ignored `.cache/viewer-password-fixture-generator.py`.

The new controller regression first proves direct Poppler rendering with the
documented byte encoding, then unlocks through the application QString facade
and checks the rendered red pixel. It would fail at the pre-repair conversion.

- Target-only Debug build, two jobs: exit 0.
- Focused CTest: **4/4**, exit 0, 4.21 seconds.
- QtTest rows: renderer **13/13**, controller **7/7**, UI **3/3**; CLI/relocated
  install smoke also passes. No failures or skips.
- Scoped production/test source-shape with warnings-as-errors and diff check:
  exit 0.

Requested next action: same reviewer checks the exact forthcoming descendant,
including the independently reproduced password case. The manager's remote
QindaMPV companion build remains next after this recheck. The original candidate
is unchanged in history; this repair is a descendant in the same worktree.
