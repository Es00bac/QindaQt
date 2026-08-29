# Rhea Calder — Display D2 repair exact review PASS

- Timestamp: 2026-08-28T12:07:28Z
- Reviewer: Rhea Calder, independent Display lead for this review
- Verdict: **PASS**
- Findings: **P0/P1/P2/P3 = 0/0/0/0**
- Exact repair: `241c00b3567463001a3eaa3f5c60ba9134cce429`
- Tree: `97d25a19a5310f36c87eec9fec58e20386a02c50`
- Parent: reviewed FAIL `8901f23fe159263522e2e0d76278c4786c8375e5`
- Scope: 13 paths, +756/-51
- Manifest SHA-256: `a3b5b78ea5e65609db24e590766c567bcb04e57f7ea0af4ac4253dc3dffab9b1`

## Finding closure

The public epoch is now `d2:<process-monotonic-lineage>:<SHA-256 bounded
restart-seed>`. Lineage increments only when a new model snapshot is accepted,
cannot wrap, survives transport loss, and is never replaced by attacker-
controlled history. The hostile row injects seed A/B/A across owners `:1.42`,
`:1.77`, and `:1.88`, retains the first A/1 candidate, proves all three public
epochs differ, and rejects the retained candidate as stale on the third
lineage. I replayed that exact pure test function from the preserved Debug
binary: the function plus QtTest setup/cleanup passed with zero failures.

The overview now accurately describes the activated cross-process read/service
foundation and its packaged fail-closed non-writer boundary.

## Private-bus source audit

Both new rows are non-vacuous. The async-source row uses three explicit
connections to one disposable bus, authenticates the initial unique owner,
holds real delayed D-Bus replies, proves dirty-read coalescing, changes owners,
accepts replacement generation 1, rejects the late old-owner reply, then proves
stop suppresses a final pending reply. The resident row proves successful
name/object registration, typed unavailable and snapshot calls, remote
`Changed`, stable epoch across revision advance, a real timer deadline/re-arm
ending in a `FullPreimage` rollback request, observer detachment, name release,
and `UnknownObject` on the still-live exact unique owner after stop.

The common fixture launches only the configured `dbus-daemon` beneath a
`QTemporaryDir` with 0700 private runtime and fresh HOME/XDG roots. It removes
inherited session-bus, X11, Wayland and Xauthority variables from the daemon;
every participant receives the explicit returned private address. Both tests
disconnect their named connections, the bus terminates then kills boundedly if
needed, and the temporary root is RAII-owned. Current inspection found no D2
daemon or disposable root. No host session/display/input/config/hardware or
installed resident path is used.

## Evidence

- Exact archive: docs/navigation 57, source shape 971 with zero findings, XML
  parse and whitespace pass.
- Directly inspected Debug and sanitizer LastTest logs show all five rows pass;
  Release CTest cost data retains all five rows with two successful observations
  each (the later registration-only command replaced its LastTest body).
- Staged surface has exactly 28 product files: 19 headers, five libraries,
  resident executable, activation descriptor, systemd unit and exact XML.
  Descriptor names/paths match, XML bytes match source, and the linked staged
  D0-decode/D1-projection consumer exits 0.
- Registrations are serial and labelled `private-dbus;isolated-runtime` for the
  two executable lifecycle rows. Dependency and new-file SPDX scans pass.

I did not compile, launch a private bus, run either private row, connect to a
host service/display/input endpoint, or edit Kellan's clean worktree. This PASS
accepts the exact repair; its current-base merge is reviewed separately.
