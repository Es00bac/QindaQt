# Kellan Ward — Display D2 exact-review repair candidate handoff

- Timestamp: 2026-08-28T11:50:19Z
- Status: handoff/not live
- Candidate: `241c00b3567463001a3eaa3f5c60ba9134cce429`
- Tree: `97d25a19a5310f36c87eec9fec58e20386a02c50`
- Parent: failed immutable candidate
  `8901f23fe159263522e2e0d76278c4786c8375e5`
- Original exact base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Scope: exactly 13 paths, +756/−51
- Sorted path-manifest SHA-256:
  `a3b5b78ea5e65609db24e590766c567bcb04e57f7ea0af4ac4253dc3dffab9b1`
- Worktree: clean

## Repaired outcome

This non-amended descendant closes every finding in Dorian's exact FAIL
`1787916255`:

1. Every accepted public epoch is now derived from a bounded restart-unique
   seed digest plus the next process-monotonic machine lineage. A repeated
   owner/factory A/B/A sequence cannot recreate an accepted public fence, the
   model retains no attacker-controlled history, exhaustion rejects, and the
   existing outer machine-lineage plus D1 token completion fence remains.
   The hostile model row retains the first A/1 candidate, accepts three owners
   under seed A/B/A, proves all public epochs differ, and proves the retained
   first candidate is stale on the third lineage.
2. The normative architecture overview now describes the actual activated,
   cross-process Display1 read/service foundation and explicitly preserves the
   packaged fail-closed non-writer stopping point.
3. Two new serial isolated-runtime rows execute the exact-owner async source
   and successful resident lifecycle beneath a disposable private D-Bus/XDG
   root. They cover initial owner read, delayed dirty coalescing, owner
   replacement/unavailability, old-owner late-reply rejection, stop
   suppression, name/object registration, typed unavailable and snapshot
   replies, remote `Changed`, two deadline fires/re-arm into a full-preimage
   rollback request, and observer/name/object teardown.

The private fixture removes inherited session-bus/display variables from its
daemon and gives every participant only the returned explicit private address.
It launches/reaps only `dbus-daemon`; the installed resident, KWin,
Wayland/XWayland, GUI/input, host service/config, display, and hardware paths
never run.

## Qualification evidence

- Fresh strict Debug configure: exit 0; five focused targets built 64/64
  serially. Final exact selector passed 5/5, including both private-bus rows.
- Fresh strict Release configure: exit 0; the same targets built 64/64
  serially. Final exact selector passed 5/5.
- Fresh ASan+UBSan configure/build: 64/64 serially. With
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, the same selector passed
  5/5.
- After the final ownership-comment-only header edit, incremental Debug,
  Release, and sanitizer builds all exited 0 and each exact selector again
  passed 5/5.
- Each private-runtime execution left zero matching display D-Bus daemon
  processes and zero disposable roots.
- Staged Release surface contains exactly 28 product files: five Display
  libraries, 19 public headers, resident executable, activation descriptor,
  systemd user unit, and Display1 XML. All four D2 headers compile first; a
  linked D0-decode/D1-projection consumer exits 0; installed descriptor/XML
  bytes match their generated/source authorities.
- `uv run --with-requirements docs/requirements.txt python -m mkdocs build
  --strict --site-dir build/d2-review-repair-docs-1787917548`: exit 0.
- `python tools/docs_validation.py`: 57 documents/navigation pass.
- `./tools/check-source-shape`: 971 source files, zero findings.
- `git diff --check`, Display1 XML parse, exact five-row CTest registration,
  SPDX, forbidden-dependency/host-boundary, manifest and clean-state gates:
  pass.

One initial first-include command under zsh failed before compilation because a
scalar containing multiple `pkg-config` flags was passed as one include
argument. The corrected Bash word-splitting invocation passed all four headers
and the linked consumer; this was a bounded command-construction failure, not a
source/package diagnostic.

## Remaining boundary and requested action

The packaged transaction port remains unavailable and fail closed. This commit
does not add a KWin/Wayland writer, journal persistence, lock/logind, Settings,
QML/shell/client, nested compositor, display, or hardware evidence.

Dorian should rereview exact commit
`241c00b3567463001a3eaa3f5c60ba9134cce429`, not this summary, bounded to exact
identity/scope, P1 A/B/A lineage closure, the corrected normative overview, the
two executable private-runtime rows and any regression introduced by these 13
paths. The compiler/private-runtime lane is released; Kellan remains available
for an exact reproduced repair.
