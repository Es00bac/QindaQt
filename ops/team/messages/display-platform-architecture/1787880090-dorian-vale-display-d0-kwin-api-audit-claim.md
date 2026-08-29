# Dorian Vale claim: Display D0 exact KWin API counterexample audit

- **Timestamp:** 2026-08-28T01:21:30Z
- **From:** Dorian Vale (`/root/display_d0/kwin_api_audit`)
- **To:** Rhea Calder (`/root/display_d0`), manager/router
- **Authority:** `1787879584-manager-display-d0-outcome.md`, accepted
  `1787859005-manager-fable-display-decision.md`, and D0 rows in
  `1787858968-elara-finch-fable-analysis-handoff.md`
- **Exact product base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Scope:** read-only installed/upstream KWin 6.6.5 ABI and implementation
  audit of `Application::outputBackend`, virtual output add/remove backend
  semantics, inventory field truth/order/uuid, removal lifetime, generation
  projection, and teardown/event-loop hazards

I claim only this bounded counterexample lane. I will not edit product source,
tests, docs, CMake, or Git; configure/build/compile/run tests; launch a
compositor; touch host session/input/display/config; or read/write KWin's
persistent output store. Durable writes remain confined to my own worker record
and new central-board replies.

Initial evidence already disproves treating the exported virtual-output methods
as a uniform success/failure capability: the base implementation returns null
on create and asserts on non-null remove, while the 6.6.5 Wayland backend's
create override does not append the returned output or emit hotplug signals.
The final handoff will give exact tag/commit, installed-header/symbol, and
upstream file/line evidence plus bounded implementation consequences.
