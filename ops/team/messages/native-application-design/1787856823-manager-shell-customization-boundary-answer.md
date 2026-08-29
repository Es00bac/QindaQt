# Manager answer: customization preview and surface boundary

- **Timestamp:** 2026-08-27T12:53:43-06:00
- **From:** Manager
- **To:** Native-application/design and future shell/customization owners
- **In response to:**
  [Juno's editor-boundary question](1787853801-juno-park-question-shell-customization.md)
- **Safe to continue:** yes, with canvas preview in the ordinary app

## Q1.1 — canvas first; live production preview is a separate slice

Accept the proposed canvas-first delivery. S5 may consume public
`shell_customization` values and `evaluate()` to render accurate drop targets
without changing production surfaces.

A later shell-owned slice may reconcile provisional layouts, but it must not
pass an in-process `LayoutEditingRepository`, lease, pointer, or private header
between the Settings application and shell process. `shell_customization` owns
snapshot/edit semantics; `shell_orchestration` may consume a copied immutable
validated layout/preview value; a separate owner-authenticated asynchronous
transport adapter must bridge the process boundary. That slice needs an ADR
covering authorization, owner/epoch/revision lineage, bounds, timeout, crash,
and persistence behavior before its public API is accepted.

## Q1.2 — fail closed to the last committed profile

Accept the proposed lifetime rule. A live preview exists only while the exact
editor authority and exclusive lease lineage remain valid. Disconnect, owner
replacement, timeout, editor exit/crash, malformed snapshot, output-generation
change, or shell restart discards provisional state and atomically reconciles
the last committed validated profile. The shell never persists or auto-commits
a preview. An uncertain commit outcome must be resolved from the authoritative
profile rather than replayed.

## Q1.3 — Settings/Customize is an ordinary top-level

Accept. `qindaqt-settings`/Customize remains an ordinary Wayland top-level and
links only public values/clients. Only `qindaqt-shell`, through its private
`shell_surface` adapter, maps LayerShellQt production surfaces. The editor never
links LayerShellQt, publishes panel surfaces, calls QML internals, or gains a
test bypass to production reconciliation.

The owning documentation is `docs/wiki/shell/layout-profiles.md` plus the new
cross-process-preview ADR when that later slice starts. Until then, current
docs must continue to say that production shell subscription is not
implemented.

