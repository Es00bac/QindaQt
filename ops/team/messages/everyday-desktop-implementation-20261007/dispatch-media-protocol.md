# ED-04a public media protocol implementation

- Time: 2026-10-07T18:12:50+00:00
- Owner: Everyday Files Sol
- Exact base: 0d15023cc31f7b46e31a8fa3447c80aa7624505c
- Worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-media-protocol-20261007
- Branch: worker/everyday-media-protocol-20261007
- Design: Accepted ADR0350 after independent d283465c3; follow its bounds and authority separation.
- Outcome: implement the standalone typed value/strict canonical wire boundary needed to share device inventory safely. This slice does not deliver consumer UI or claim ED04 complete.

Own new src/services/removable_media_protocol module: typed owning snapshot, row, admission/action/result and diagnostic values, canonical little-endian envelope codecs and explicit version/message kind and all ADR bounds. No connections, UDisks, persistence, process launch, policy decisions, presentation or backend mutation. Keep codec internals private; separate snapshot from action/result responsibilities if nonblank size reaches decomposition threshold. Public API documents ownership, threading, error and compatibility behavior. Reuse existing repository patterns, no new dependency.

Own focused tests under tests/services/removable_media_protocol, split valid round-trip/canonicality from malformed/adversarial payloads. Meaningful negative cases: unknown version/enums/Boolean; invalid UTF8/trailing bytes/truncation; duplicate identities; length/count/allocation bounds; mount roots/identifiers; correct message-kind isolation. Roundtrip boundary values and error behavior. A successful parse never turns presentation paths/ids into privileged authority. All snapshot data owning copies, not borrowed views.

Minimal additive shared coordination edits approved: src/services/CMakeLists.txt and top-level CMakeLists.txt only for module/tests if existing pattern needs them; installed target/header registration only as module boundary requires. Add planned protocol row in docs/wiki/architecture/module-boundaries.md and keep Accepted ADR0350 / RemovableMedia wiki accurate about what is implemented and what is next. Do not mark public client/exporter/sidebar/chooser delivered. New documentation only if necessary; maintain mkdocs navigation. No unrelated formatting/registry edits.

Read normative module-boundaries/coding/testing pages and current protocol examples first. Source/fixture authoring may run now. Compiler lease belongs to Platform signed package; do not compile/configure heavy full tree or run private fixtures until root grants after release. Exact configured MAKEOPTS -j24 -l24; root speaks before every actual build/test batch. No host devices/services/session/config/software installation. Use existing Qt/toolchain on qinda.

Self-authored ed-files-sol live record and timestamped claim/findings/verification/handoff under everyday-desktop-implementation-20261007. Preserve exact candidate in explicit /home/cabewse/git/container-wm.git hub. Same independent reviewer before integration; count exact tests/commands and bounded caveats. Root owns integration/queue/task and artifact metadata. Finish one exact protocol candidate; no further module implementation until followup.
