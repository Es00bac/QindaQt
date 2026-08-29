# Mira Quill finding: the surface row races intended intelligent hiding

- **Timestamp:** 2026-08-28T02:54:40Z
- **Status:** source diagnosis established; repair design ready for review
- **Exact base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`

The repeat is not a Controls defect and is not evidence that the bottom surface
failed its initial Wayland handshake. Cora's result proves that the bottom
surface configured, acknowledged, attached buffer `49`, and committed a causal
mapped epoch with the expected 998 x 54 size and exclusive zone 54. It then
received a pending exclusive zone of `-1`, destroyed its layer role, and became
unmapped.

That transition is the implemented product policy:

- `data/profiles/qindaqt.json` gives `smart-shelf` `hideMode: intelligent`.
- `tests/session/shellsurfaceprobe.cpp` maximizes the ordinary probe before it
  starts `qindaqt-shell`.
- `src/shell_visibility/src/panel_visibility_policy.cpp` hides an intelligent
  panel for an overlapping active or maximized window.
- `src/shell/runtime/shellruntimeapplication.cpp` initially publishes the
  all-visible fallback, then reconciles the owner-bound compositor snapshot.
- `src/shell_surface/src/layer_shell_surface_backend.cpp` releases the hidden
  panel's reservation and calls `QQuickWindow::hide()`.

The nested row therefore races its historical initial-publication assertion
against the later live visibility generation added by `1246fea`. The accepted
wiki already says this matrix predates and does not prove the live automatic-
hide transition. A fast visibility reply hides the shelf before the probe's
50 ms poll observes two mapped roles; slower historical runs can pass, making
the current test timing-dependent.

The narrow repair should keep the production executable and protocol/work-area
proof but run it with a checked-in, schema-valid two-panel proof profile whose
two hide modes are `never`. That removes the unrelated automatic-hide policy
from this row without adding a test-only production bypass, changing the real
QindaQt default profile, weakening the exact layer protocol checks, or
enabling compositor mutation. The probe should receive the proof profile ID
explicitly rather than hard-code the user-facing `qindaqt` profile. The wiki
must describe the deterministic proof profile and retain the explicit boundary
that live automatic-hide protocol transitions remain separate acceptance work.

No source has been edited yet and no configure/build/test process has run.
Cora Vale still owns the compiler/test lane.
