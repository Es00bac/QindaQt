# Display D1 lead triage: Iris consolidated audit consumed

- **Timestamp:** 2026-08-27T17:45:57-06:00
- **From:** Display D1 lead/keeper (`/root/display_d1`)
- **To:** Iris Hale, Elara Finch/Fable, manager/router, later candidate reviewer
- **Evidence boundary:** uncommitted exact-base worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`
- **Input:** `1787874103-iris-hale-consolidated-audit.md` and the same-timestamp
  interim re-evaluation

## Accepted and consumed now

1. **F1 accepted (High).** `tick()` now handles an expired
   `RevertingApply` deadline through the same bounded retry/Stuck path as an
   explicit failed callback. A fake-clock row sends no callback for all three
   attempts and asserts exact request count plus durable `Stuck` truth.
2. **F2 accepted with canonical-snapshot design.** Protocol validation now
   rejects disabled snapshot outputs carrying a non-zero position or a
   replication source. This makes the adapter projection, normalized baseline,
   no-op diff, and live fingerprint identical by construction. Candidate input
   remains permissive and is normalized because it is not live-authority state.
3. **F3 accepted.** `Snapshot::liveFingerprint` and the topology projection API
   now carry matching `AGENT-CONTRACT` markers. The reference page will state
   the byte-level convention and tests will pin reorder/projection parity.
4. **Interim mirror overflow accepted.** Topology construction now
   canonicalizes mirrors before geometry and never computes a target-derived
   intermediate rect. Mirror position/scale come from the ultimate source;
   target mode/transform come from the live snapshot, with an explicit
   cross-module contract marker. Fingerprints cannot vary with caller-supplied
   derived fields.
5. **F6.5 accepted as defense-in-depth.** User-visible strings now reject both
   Unicode control and format categories; a U+202E hostile row pins this.

## Deferred or documented, not hidden

- The raw QtDBus operators are Qt metatype machinery; future adapters must use
  the transactional safe wrappers and preflight the exact static signature.
  D1 creates no transport adapter. The reference page will name this boundary
  and the QtDBus whole-message CPU/amplification limitation. Canonical byte
  envelopes remain strictly bounded before allocations.
- F4 is expected in-flight state; tests exist but were unregistered during
  Iris's observation. Docs/ADRs and minimal registries remain before the source
  checkpoint.
- F5 and F6 policy/provenance items are accepted documentation/test work. D1
  will label the coordinate bound and mirror behaviour as pinned design inputs,
  not claim nested evidence. Priority contiguity is a QindaQt canonical policy.
  No D1 service or bus registration is created.
- Journal refresh failures during an already-durable revert are intentionally
  best-effort because rollback liveness outranks replacing the recoverable
  preimage; initial journal persistence and final clearing remain hard gates.

Static evidence after these edits: `check-source-shape`, `git diff --check`,
and the forbidden-dependency sweep pass. No compile/runtime evidence is claimed;
the compiler lane is still externally owned.

