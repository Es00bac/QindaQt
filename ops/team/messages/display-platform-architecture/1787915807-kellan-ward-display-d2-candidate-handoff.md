# Kellan Ward — Display D2 immutable candidate handoff

- Timestamp: 2026-08-28T11:16:47Z
- Candidate: `8901f23fe159263522e2e0d76278c4786c8375e5`
- Tree: `0b2bcee3178ab34283b3e64714933b2ca7a57ccc`
- Exact parent/base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Branch/worktree: `worker/display-d2`, `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Scope: exactly 27 paths, +2,776/−25; worktree clean
- Status: handoff/not live; exact different-worker review requested

## Outcome

This candidate adds the bounded resident `org.qindaqt.Display1` foundation over
the integrated D0 inventory and accepted D1 public modules. It owns:

- an exact-unique-owner asynchronous `Compositor1.Outputs` reader with one
  serialized read, owner/reply serial fencing, signal coalescing, and loss;
- bounded schema-1 JSON decode, integral geometry, strict D1 scale/output/text
  limits, privacy-preserving connector-only identity projection, one observed
  current mode, and canonical topology fingerprinting;
- deterministic owner/epoch/revision behavior: equal generation requires exact
  typed frame equality, regressions and unjustified increments reject, owner
  replacement/loss clears the snapshot and machine, and recovery takes a fresh
  injected epoch;
- an injected D1 transaction composition that routes add/remove/change,
  staged external intent, preview/confirm/cancel/revert, timer deadlines, and
  apply completions; a request-scoped outer machine lineage prevents an old
  completion from colliding with a numerically reused D1 token after loss;
- the resident Qt Core/DBus object, activation descriptor, hardened systemd
  user unit, Display1 XML, package install surface, focused tests, and the
  owning architecture/reference/module-boundary documentation.

The packaged transaction port is deliberately unavailable. Safety begins
`Unknown`; it cannot store/clear a journal or issue a compositor request. This
is a fail-closed resident/read/service foundation, not a simulated output
writer.

## Qualification evidence

- Fresh strict Debug configure exit 0. After repairing two initial compile
  diagnostics (QObject constructor accidentally exposed as a slot; Qt
  `signals` macro used as a test local), the focused target build completed
  60/60. The final hostile-text decoder addition exposed one missing direct
  `display_validation.h` include; after that bounded repair the incremental
  target build completed 18/18. Final
  `ctest -R '^qindaqt\.display-service-' --parallel 1` passed 3/3, exit 0.
- Fresh strict Release configure and focused build completed 60/60, exit 0;
  after the final decoder repair the incremental build completed 21/21. The
  same CTest selector passed 3/3, exit 0.
- Fresh ASan+UBSan focused dependency/test build completed, then the final
  incremental build completed 19/19. With
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, the same selector passed
  3/3, exit 0.
- Display-only staged install produced exactly 28 product files: five static
  Display libraries, 19 public headers, the resident executable, activation
  descriptor, user unit, and XML. Each of the four D2 headers compiled as a
  first include. A staged linked consumer exercised D0 decoding and D1
  projection and exited 0.
- The exact staged user-unit bytes were mirrored only inside the ignored
  staging root so `systemd-analyze --root ... --recursive-errors=no verify`
  could resolve the staged absolute executable; final verification exited 0.
  Earlier direct `--root`/`--user` lookup attempts exited 1 because systemd
  could not locate the user unit/dependencies, not because of a unit-content
  diagnostic.
- `uv run --with-requirements docs/requirements.txt python -m mkdocs build
  --strict` exited 0. The bare `mkdocs` executable was absent, so no result is
  claimed for that unavailable invocation.
- `tools/docs_validation.py`: 57 documents/navigation pass.
- source-shape: 968 source files, zero issue; largest D2 production source is
  423 non-blank lines.
- `git diff --check`, Display1 XML parse, exact descriptor name/path parity,
  public-header manifest, forbidden dependency, machine-path, SPDX source,
  and staged-manifest gates pass. The candidate worktree is clean.

No resident executable, session/private bus, nested compositor, display/input
path, host service, or hardware runtime was launched or touched.

## Bounded stopping point

This commit intentionally does not add the public KDE output-management port,
durable journal/lock, crash recovery, lock/logind adapters, Settings registry,
typed client, Settings/shell UI, nested preview/confirm/revert convergence, or
physical/mixed-output hardware qualification. It has no KWin header/private
ABI, Wayland object, QML, Settings implementation, filesystem persistence,
logind, libkscreen, or shell dependency.

## Integration and requested review

The locally visible `origin/main` is `b62e132e067842b51f95aeaa377efef1dfda9bc5`.
Its 11 post-base paths are Power/roadmap/board documentation and have zero path
intersection with this candidate. The manager's newer integrated Notification
tree may still touch shared registries not yet visible in that ref. Preserve
both additive `src/CMakeLists.txt`/`tests/CMakeLists.txt` lines and the existing
architecture prose; all D2 product/test paths are isolated.

A different worker should review the exact candidate SHA, not this summary,
for P0–P3 findings. Please verify commit/tree/parent/scope; hostile JSON and
unique-owner fencing; equal-generation content rejection; owner/loss reset;
identity privacy and canonical projection; request-scoped outer callback
lineage; resident ownership/teardown/timer and D-Bus method/error behavior;
descriptor/XML/install parity; public header self-containment; fail-closed
packaged mutation; and documentation/stopping-point truth. Route a reproduced
defect back to Kellan in this worktree for a non-amended descendant.
