# Maxwell the 3rd — Bluetooth B0 manager replay exact-review PASS

- Time: 2026-08-28T20:52:39Z
- Reviewer: Maxwell the 3rd, OpenAI collaboration runtime; exact serving model
  and reasoning are unexposed and were not inferred.
- Exact candidate: `f38707b6f7fa2a26a6e3748fe86dd0ccc064aea7`
- Exact tree: `20b90862537ae317d1301986ef3079eab956e833`
- Exact parent: `bb2c12919c75c1e98a0cd5ad3d746611bbc18a94`
- Exact manager base: `f783f8389a563423e6e6bf2d98bd276748657a1e`
- Verdict: **PASS — integrate immediately**
- Findings: **P0/P1/P2/P3 = 0/0/0/0**

## Replay and manager-union proof

Manager base `f783f83` is the exact ancestor of a linear five-commit range.
The replay map is `f94353d6 -> 3cf6f09d`, `bbbe8b8f -> 3ec7b37d`,
`e19d094c -> 0edba79a`, `f810108b -> bb2c1291`, and
`278a5f95 -> f38707b6`. Every pair preserves author name/email, author time,
subject, and body exactly. Range-diff marks the foundation and final two repair
commits exact; the two middle differences are only the expected current-manager
shared-context union around the ADR renumber.

Relative to the exact manager base, the candidate has 59 paths, zero deletions,
zero `ops/team/**` paths, and a clean diff. Every one of the 54 non-shared
Bluetooth paths has the exact blob ID from independently accepted candidate
`278a5f9520f3cc47e554816961c0c653295fcbc4`. The only five shared paths are
strict additions-only manager unions:

- `docs/wiki/adr/index.md`: `+1/-0`
- `docs/wiki/architecture/module-boundaries.md`: `+10/-0`
- `mkdocs.yml`: `+3/-0`
- `src/CMakeLists.txt`: `+4/-0`
- `tests/CMakeLists.txt`: `+4/-0`

Those additions are Bluetooth-only; the manager's Font, Settings Center, Text
Editor, File Manager, tray, audio, clipboard, display, notifications, shell,
and documentation registrations remain byte-present.

## Fresh executable evidence

| Gate | Result |
| --- | --- |
| Strict Debug configure (`CMAKE_AUTOMOC_PATH_PREFIX=ON`) | exit 0 |
| Focused Bluetooth production/test build plus the two install-graph executables | exit 0; final rebuild reports no work |
| Exact selector `ctest -R '^qindaqt\\.bluetooth' --no-tests=error` | exit 0, **9/9**, including private-D-Bus, activation, owner-loss, and staged install |
| Eight direct executables | **16+17+11+12+4+3+4+3 = 70 passed, 0 failed** |
| Host-session poison | Qt transport hostile-wire, activation/restart, and caller-owner-loss rows all pass against their own private buses with `DBUS_SESSION_BUS_ADDRESS` set to a nonexistent path |
| Focused policy rows | fixed signatures, real marshalling, oversized collections, projected lease caps, pre-publication epoch advance, fetch-failure authority revocation, owner replacement uncertainty, and queued completion all pass |
| Staged payload | executable, activation descriptor, hardening unit, XML, protocol archive, and installed linked consumer present; consumer exit 0; no template placeholders |
| Package poison | hiding the built Bluetooth protocol archive makes the registered staged row fail exit 8 at its exact missing artifact; restoring the same archive returns the row to PASS |
| Source shape | exit 0, 1,402 files, 0 allowlisted |
| Documentation | `validate-docs` exit 0, 93 Markdown documents plus nav; strict MkDocs exit 0 |
| Integrity | `diff --check`, conflict-marker scan, `git fsck --no-dangling --strict`, identity, and final clean status all pass |

The first exact selector invocation was 8/9 because a fresh focused build had
not linked an unrelated whole-tree install artifact (`libqindaqt_profiles.a`).
I built the unchanged candidate's installable product graph, deliberately
stopped the later broad test-target expansion at 889/1914 because it is outside
this review, and then obtained two clean final 9/9 runs. That setup miss is not
a product finding and no broad all-target build is claimed.

Faraday Stone's six source mutations on accepted `278a5f95` remain
cryptographically applicable to this replay because all 54 non-shared blobs,
including every production and test byte, are identical. This review also
independently established the package negative control and private-bus host
poison without editing candidate source.

## Bounded qualification boundary

This PASS qualifies the B0 typed protocol/model/client/resident-service,
private-bus lifecycle and activation behavior, deterministic unavailable
backend, and packaged public boundary. It does not qualify BlueZ, physical
adapters, rfkill, pairing Agent1 UX, Bluetooth audio correlation,
suspend/resume, hotplug, resource budgets, or Settings/shell UI. No host
Bluetooth, radio, session bus, display, input, desktop, or configuration was
touched.

## Next action

The Program Manager should integrate exact replay candidate
`f38707b6f7fa2a26a6e3748fe86dd0ccc064aea7` immediately, rerun the affected
gates on the integrated tree, and advance QQ-005.05 only from that integrated
evidence. There are no blocking findings to route back to Lovelace.
