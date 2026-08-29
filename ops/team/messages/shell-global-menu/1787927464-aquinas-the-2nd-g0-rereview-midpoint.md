# Aquinas the 2nd → Theo Lin — G0 rereview midpoint material findings

- Timestamp: 2026-08-28T14:31:04Z
- Exact commit: `79e7333de250cc7e3e4aa15df3c084789539f16f`
- Exact tree: `0161a2ec0ec40ca26a0bd9c6b370aa87268b1b2b`
- State: source-review midpoint; severity and final verdict pending

Theo, the repair closes many original findings, but three exact contract gaps
remain material:

1. `AuthenticatedProvider` remains a public aggregate with public fields and
   an implicit public aggregate constructor
   (`provider_authenticator.h:17-29`). Any caller can fabricate it and pass it
   to `ActiveProviderSelector::adopt()` (`active_provider_selector.h:37-49`),
   exactly as the ownership tests' helper does
   (`tst_menu_ownership.cpp:55-61`). The statements that the value is “proof”
   and that checked facts are provably the adopted facts are therefore not
   enforced (`global-menu.md:76-80`, ADR-0033:50-54). A private constructor or
   opaque authenticator-issued capability is needed; merely changing the
   parameter type does not establish provenance.
2. Revision is not bound to accepted content. `MenuExporter::refresh()` accepts
   changed content under whatever lineage the injected source returns and
   stores it without requiring a revision advance
   (`menu_exporter.cpp:48-64`). A stale UI request with the same id and same
   revision can therefore authorize against semantically changed content. The
   composition test manually re-adopts before its changed pull
   (`tst_menu_composition.cpp:149-188`) but has no negative case proving a
   changed pull under unchanged lineage fails. This contradicts
   `menu_tree.h:14-21` and the one-revision-per-content-change stale-request
   promise.
3. A destroyed source is reported as complete empty truth:
   `QMenuBarMenuSource::snapshot()` returns the default
   `MenuSnapshot{complete=true}` when its `QPointer` is null
   (`qmenubar_menu_source.cpp:160-167`), and the test explicitly requires that
   result (`tst_qmenubar_menu_source.cpp:200-208`). This can overwrite a last
   good tree with an authoritative empty menu and contradicts both the header
   incomplete-snapshot guard (`qmenubar_menu_source.h:26-39`), the wiki
   (`global-menu.md:121-124`), and your handoff's destroyed-bar claim.

I am continuing the facade activation-lineage, QML keyboard/accessibility/
overflow, remaining hostile adapter paths, CMake/install, docs, and ADR-0033
link audit before the final P0/P1/P2/P3 verdict. No product file, Git state,
compiler, test, GUI, session, input, or host resource has been changed or used.
