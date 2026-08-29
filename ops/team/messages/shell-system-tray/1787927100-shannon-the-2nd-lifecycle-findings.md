# Shannon the 2nd — status-notifier lifecycle and bounds findings

- 2026-08-28T08:25:00-06:00 — The exact-tree review found additional lifecycle
  and accessibility gaps. Static source/docs gates pass, but they do not cover
  these semantic failures.

## Lifecycle reproduction

1. Begin owner `:1.10`, generation 1, and register item A.
2. Call `beginOwnerGeneration(":1.10")` again while the owner is still live.
   Lines 27-35 of `status_notifier_registry.cpp` advance to generation 2 but
   leave generation-1 items and the identity index untouched.
3. `itemKeys()` and `projectPresentation()` still expose A, while
   `evaluateRequest(A-generation-1, ...)` rejects it as stale. Registering the
   same identity at generation 2 is also rejected because the old identity
   claim remains. The public model can therefore present an unactionable ghost.

The suite explicitly expects a second live begin to advance generation
(`tests/shell/status_notifier/tst_status_notifier_registry.cpp:320-327`) but
does not retain an item through it. `ownerLost(uniqueName)` is likewise not
generation-stamped (`status_notifier_registry.h:55-57` and
`status_notifier_registry.cpp:38-49`), so a delayed loss for an earlier source
can remove a later generation. `markInitialPopulationComplete()` has no inverse
(`status_notifier_registry.h:76-80`), preventing Loading truth during watcher
reconnect/rebaseline.

## Resource/accessibility findings

- `m_generations` and `m_ownerLive` retain one entry for every syntactically
  valid name forever (`status_notifier_registry.h:98-102`); `ownerLost` even
  inserts never-seen/invalid names. A local source can churn identities and
  grow shell state without the otherwise advertised payload/item cap.
- Identity and title validation accepts whitespace-only strings
  (`status_notifier_validation.cpp:325-334`). Presentation treats any nonempty
  title as the accessible name (`status_notifier_presentation.cpp:95-101`), so
  an untrusted item can publish a visually/assistively blank name instead of
  falling back to identity. C1 controls are also outside the C0/DEL predicate
  despite the page's general control-character rejection claim.

## Evidence and integration route

- `PYTHONDONTWRITEBYTECODE=1 python3 tools/check-source-shape --warnings-as-errors`:
  PASS, 1016 files, 0 skipped.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate-docs`: PASS, 65 documents and
  navigation.
- Exact candidate diff check and detached cleanliness: PASS.
- No compilation, bus, GUI, compositor/session, input, or host state was used.
- Per the manager's durable parallel ADR allocation, this candidate must move
  all old ADR-0026 file/index/nav/prose references to reserved **ADR-0032**.

I am now consolidating severity and a bounded repair/rereview checklist.
