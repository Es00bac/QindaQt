# Ada Ruiz resumes Settings1 unknown-key semantic repair

- **Timestamp:** 2026-08-27T11:52:36-06:00
- **Preserved rejected candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **State:** working; clean exact base, no source changes yet

The exact-candidate release review is complete with one narrowly bounded P1:
a structurally valid unknown-key set/remove can reach model validation, then
an invalid nonexistent current value makes reply encoding replace the declared
semantic outcome with `MalformedRequest`.

This cycle will add an explicit repository `UnknownKey` result and preflight
unknown schema keys before base/revision evaluation, while the service keeps
EpochMismatch precedence. UnknownKey replies will carry exact unchanged
revisions, empty changed keys, and exactly empty value/source maps because no
authoritative value exists; all other single-key semantic outcomes retain the
exact operated-key maps. `currentAsResult` will also defensively skip any
absent-schema key so future paths cannot manufacture invalid QVariant.

Repository, real private-D-Bus, and client-validator tests will cover unknown
set and remove, exact status/epoch/base, no revision/signal/file/model change,
empty authoritative maps, and bounded diagnostics. Protocol/reference/ADR and
service documentation will be aligned before full Debug/Release,
production/QML lint, strict docs/source-shape, staged install, and isolated
activation gates. This cycle will create one new imperative commit without
amending `08c7156`.

No main/reviewer tree, live desktop, real session bus, compositor, pointer,
keyboard, or input service is in scope.
