# Lovelace the 3rd — Bluetooth B0 manager replay handoff

- Time: 2026-08-28T14:40:11-06:00
- Outcome: clean isolated replay of the complete independently accepted Bluetooth B0 series onto the current manager boundary, preserving every newer manager feature.
- Worktree/branch: `/mnt/d/QindaQt/worktrees/bluetooth-manager-replay-lovelace3`, `worker/bluetooth-manager-replay-lovelace3`.
- Manager base: `f783f8389a563423e6e6bf2d98bd276748657a1e`.
- Candidate tip: `f38707b6f7fa2a26a6e3748fe86dd0ccc064aea7`.
- Candidate tree: `20b90862537ae317d1301986ef3079eab956e833`.
- Candidate parent: `bb2c12919c75c1e98a0cd5ad3d746611bbc18a94`.
- Status: handoff — requesting a different worker's exact replay review before manager integration.

## Exact replay lineage

The five accepted commits were replayed in order without squashing:

| Accepted commit | Replay commit | Outcome |
| --- | --- | --- |
| `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c` | `3cf6f09dd5968ca56867a16d6697f5d4b9ca43f7` | Bluetooth B0 foundation |
| `bbbe8b8f6f5e51033da857e3b0c6d38dc442fbb4` | `3ec7b37d49fedded38a3015e24922f68664d42c8` | Least-authority repair |
| `e19d094c792d132d3d65129056281ca556415c0f` | `0edba79aa2e1ef33e2c8884841921e291675631d` | ADR-0037 renumber |
| `f810108b4042b2215a318f48430de743b883d51a` | `bb2c12919c75c1e98a0cd5ad3d746611bbc18a94` | Lineage/lease/client hardening |
| `278a5f9520f3cc47e554816961c0c653295fcbc4` | `f38707b6f7fa2a26a6e3748fe86dd0ccc064aea7` | Hostile-wire repair and doc polish |

Faraday Stone's independent non-Gemini acceptance of original tip `278a5f9`
is in `platform-bluetooth/2026-08-28T203400Z-faraday-stone-b0-exact-rereview-pass.md`
with verdict P0/P1/P2/P3 `0/0/0/1`.

## Changed-path and union proof

The replay changes 59 paths relative to `f783f83`:

- Bluetooth documentation: `docs/wiki/architecture/bluetooth-service.md`,
  `docs/wiki/reference/bluetooth1-v1.md`, and
  `docs/wiki/adr/0037-keep-pairing-and-trust-authority-in-bluez.md`.
- Bluetooth production modules: every tracked path under
  `src/services/bluetooth_protocol/`, `bluetooth_model/`,
  `bluetooth_client/`, and `bluetooth_service/`.
- Bluetooth tests/package proof: every tracked path under
  `tests/services/bluetooth_protocol/`, `bluetooth_model/`,
  `bluetooth_client/`, and `bluetooth_service/`.
- Shared reconciliation points only: `docs/wiki/adr/index.md`,
  `docs/wiki/architecture/module-boundaries.md`, `mkdocs.yml`,
  `src/CMakeLists.txt`, and `tests/CMakeLists.txt`.

All 54 non-shared paths are byte-identical at the replay tip to accepted
candidate `278a5f9` (`54 compared, 0 mismatches`). The five shared files are
strict additions-only unions relative to manager base: `+1/+0`, `+10/+0`,
`+3/+0`, `+4/+0`, and `+4/+0` respectively. They retain current manager
Font, Settings Center, Text Editor, File Manager, tray, audio, clipboard,
display, notification, shell, queue, and documentation registrations while
adding only Bluetooth. The candidate range contains zero `ops/team/**` paths.

## Fresh verification on the replay

| Gate | Result |
| --- | --- |
| Strict Debug configure with `-DCMAKE_AUTOMOC_PATH_PREFIX=ON` | exit 0 |
| Focused 67-step build: eight Bluetooth test executables plus `qindaqt-bluetooth-service` | exit 0, no warnings/errors |
| Unified `ctest -R '^qindaqt\\.bluetooth'` | exit 0, 9/9 including private-D-Bus rows and staged install |
| Direct executables | 16+17+11+12+4+3+4+3 = 70 passed, 0 failed |
| Staged package | exit 0; executable, activation descriptor, systemd unit, XML, protocol archive and linked installed consumer verified |
| `python3 tools/check-source-shape` | exit 0; 1,402 files checked, 0 allowlisted |
| `python3 tools/validate-docs` | exit 0; 93 Markdown documents plus navigation |
| `/home/cabewse/venv/bin/mkdocs build --strict` | exit 0 |
| Diff/content/provenance | diff check clean; base is ancestor; 54/54 accepted non-shared blobs exact; zero candidate `ops/team/**` paths |
| Final tree | `git status --porcelain` empty |

The first staged-install invocation was an evidence-setup miss, not a product
failure: I deliberately stopped the broader `all` build at 1,276/2,084 after
the Bluetooth and installable product graph had compiled, but two manager
executables had not yet linked. The install script truthfully stopped at the
missing `qindaqt-display-service`. Building exactly
`qindaqt-display-service` and `qindaqt-file-manager` linked the two pending
outputs; the unchanged candidate then passed the staged row and the final
unified 9/9 selector. The complete `all` test-target build was not claimed.

## Manual conflict record

- `docs/wiki/adr/index.md`: retained all manager ADRs, replaced the candidate's
  temporary Bluetooth ADR-0026 entry with final reserved ADR-0037.
- `docs/wiki/architecture/module-boundaries.md`: retained the manager's
  StatusNotifier dependency paragraph and added the Bluetooth1/BlueZ paragraph;
  final link points to ADR-0037.
- `mkdocs.yml`: retained all manager architecture/reference/ADR entries and
  added Bluetooth service, Bluetooth1 reference, and ADR-0037 navigation.
- `tests/CMakeLists.txt`: retained the manager audio applet and every newer
  test registration, then added the four Bluetooth test directories.
- `src/CMakeLists.txt` merged automatically and was manually verified as the
  same additions-only union for the four Bluetooth production directories.
- No other path conflicted; all Bluetooth-specific blobs match the accepted
  candidate exactly.

## Bounded caveats and next action

This B0 evidence intentionally does not qualify BlueZ, physical adapters,
rfkill/radio state, pairing Agent1 UX, Bluetooth audio correlation,
suspend/resume, hotplug, or the future Settings/shell UI. No host Bluetooth,
radio, host session bus, desktop session, or configuration was touched.

A different worker should review exact tip `f38707b6f7fa2a26a6e3748fe86dd0ccc064aea7`
against manager base `f783f8389a563423e6e6bf2d98bd276748657a1e`,
verify the 54-path equivalence and five additions-only unions, rerun a
proportional focused selector/package check, and post an exact PASS/FAIL.
After PASS, the manager may fast-forward/cherry-pick this five-commit range,
rerun integrated gates, and advance QQ-005.05 from integrated evidence only.
