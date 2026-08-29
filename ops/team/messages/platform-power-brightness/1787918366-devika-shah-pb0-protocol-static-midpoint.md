# Devika Shah PB-0 protocol static midpoint

- Timestamp: 2026-08-28T05:59:26-06:00
- Exact base: `0a547df33d9a31b969d78b4ca649d0b39dc04797`
- Boundary: PB-0 commit 1 of 3, source/tests/docs/static evidence only

The accepted architecture fixes the public collection caps exactly: eight
extra power supplies, four profiles, eight holds, eight inhibitors, eight
keyboard backlights, and eight provider backlights. It explicitly assigns the
full reference page to the implementing slice, so it does not supply numeric
UTF-8/scalar caps for every other field. This candidate fixes those remaining
v1 limits in `power_limits.h` and the new Power1 reference page rather than
leaving adapters to invent them independently.

The first vertical boundary now has separate small modules for fixed values,
semantic validation/sanitization, deterministic canonical bytes, and raw fixed
QtDBus structures. Handles carry `(epoch, opaque-id)`; raw paths/serials are not
representable. Public inhibitors contain exactly `what/who/why/mode`, so UID
and PID cannot be carried or merely hidden. Snapshot and result decoders build
a temporary, check every count/length/version/enum/lineage/known-value
invariant, require exact end-of-buffer, and replace the caller's retained value
only on total success.

Adversarial source tests cover every collection cap, UTF-8 boundaries and
control/NUL sanitization, fixed no-`a{sv}` structures, NaN/infinity/unknown
enum/capability rejection, mixed and duplicate lineage, the four accepted
fail-closed backlight identity reasons, exact Wayland binding lineage, stable
big-endian encoding, truncation/magic/trailing/oversize failures, and atomic
destination retention.

Static evidence at this exact uncommitted boundary, all exit 0:

- `git diff --check`
- `tools/check-source-shape` — 995 files, no finding
- `tools/validate-docs` — 64 pages/navigation
- `uvx --from mkdocs mkdocs build --strict`

`clang-format` mechanically normalized only owned new C++/header paths. No
configure, compiler, binary test, bus, session, runtime, compositor, hardware,
or host-state command ran because Display D2 retains that lane. The first
commit will be preserved as source/static-ready, not called executable or
accepted; focused build/tests remain mandatory after manager release.
