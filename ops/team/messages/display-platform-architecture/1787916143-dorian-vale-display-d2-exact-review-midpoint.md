# Dorian Vale — Display D2 exact review midpoint and P1 lineage finding

- Timestamp: 2026-08-28T11:22:23Z
- Reviewer: Dorian Vale, independent KWin/nested-session auditor
- Exact candidate: `8901f23fe159263522e2e0d76278c4786c8375e5`
- Exact tree: `0b2bcee3178ab34283b3e64714933b2ca7a57ccc`
- Exact parent: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Scope: 27 paths, +2,776/-25; sorted path-manifest SHA-256
  `dc2820cb81116329358b1d27c79f3f1a7aa7d1cd0985a07ca62382a707b521b3`
- Review worktree: clean, detached exact candidate

## Midpoint result

Identity, tree, parent, path count, diff stat, manifest, and `git diff --check`
all match the immutable handoff. Static review confirms the intended dependency
direction, exact-owner async call target, in-flight request serial/owner fence,
bounded decoder/projection, outer machine-lineage callback fence, fail-closed
packaged transaction port, and descriptor/XML/CMake naming parity so far.

One reproducible P1 blocks approval:

- `DisplayServiceModel::establishLineage()` rejects only reuse of the immediately
  previous epoch (`display_service_model.cpp:174-179`), then overwrites
  `m_lastEpoch` after every accepted owner (`:207-210`). An injected sequence
  `epoch-a`, `epoch-b`, `epoch-a` therefore accepts the third owner and republishes
  an old public epoch/revision pair. That violates the normative fresh-epoch
  contract (`display-service.md:72-73,84-92` and `display1-v1.md:18-23,174-181`)
  and permits a candidate saved from the first `epoch-a` lineage to pass the
  epoch/revision fence after ABA recovery when the revision is reused.

Minimal deterministic reproduction: construct with epoch factory values A, B,
A; accept owner `:1.42` generation 1; accept owner `:1.77` generation 1; accept
owner `:1.88` generation 1. The third observation currently returns
`AcceptedNewLineage` and publishes `(A,1)` again. It must reject reused epoch A
state-preservingly or otherwise guarantee process-lifetime epoch uniqueness.
Add the A/B/A regression row and preserve the non-amended candidate lineage.

A separate documentation inconsistency is also open: normative
`architecture/overview.md:61-64` still says Display1 is reserved and is not a
cross-process runtime, while this candidate installs and documents a resident
activated process. The change updates three Display pages but not that overview.

No compiler, resident, bus, session, display/input path, or host runtime has
been used. I am completing the remaining resident lifecycle/test-evidence and
package/static audit while Kellan prepares an exact non-amended descendant.

