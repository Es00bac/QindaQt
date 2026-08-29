# Ada Ruiz — Settings1 value/state repair handoff

- **Timestamp:** 2026-08-27T11:24:44-06:00
- **Preserved rejected predecessor:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Second repair candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **Tree:** clean
- **Repair size:** 36 paths; 1,723 insertions; 242 deletions

## Outcome

This one new imperative commit repairs the two P1 classes that remained after
the exact `55105b2` review, including every pre-candidate design-audit
constraint:

1. Object settings now enter one recursive canonical, restart-stable JSON
   domain: Nullptr, bool, signed 64-bit integer, finite double, well-formed
   NUL-free string, list, or map. Invalid QVariant, unsigned values above
   `INT64_MAX`, non-finite/non-JSON values, malformed UTF-16, embedded NUL, and
   invalid object keys are rejected before mutation. Persistence uses an
   explicit canonical QJson encoder rather than lossy QVariant conversion.
2. Settings1 carries canonical null only as the fixed scalar D-Bus signature
   `g:"v"`. The inbound decoder turns that exact reserved marker back into
   Nullptr; other signatures and all caller-supplied signatures are rejected.
   There is no variable-length byte-array marker and no separate unbounded
   marker pre-scan.
3. One bounded traversal applies shared node/byte/depth/container/key/string
   budgets before wire encoding. Ordinary QStringList and opaque `as`
   canonicalize identically. The decoder was decomposed into cohesive value,
   envelope, and encoder collaborators; the largest repaired decoder is 423
   nonblank lines.
4. Every snapshot and commit-outcome value map is encoded before QtDBus. The
   public `QtSettingsTransport::commit` validates and encodes the complete
   operation envelope before libdbus sees it, so direct invalid/null callers
   cannot abort. Resident startup rejects schema/profile/user layers that
   exceed Settings1 wire fit.
5. Exact-metatype tests carry nested null, signed limits, accepted unsigned
   values, arrays/maps, `-0.0`, denormal/minuscule values, maximum double,
   `0.1`, and `nextafter` boundaries around both sides of 2^63 through schema,
   save/reload, real private D-Bus, persisted file, and service replacement.
   Malformed Unicode/NUL/collision and too-wide/non-finite cases reject without
   mutation or overwriting the previous file.
6. DoNotDisturb authority loss now dominates accepted-save and Conflict
   projections, while valid conflict/request intent is restored only after a
   fresh authoritative baseline. Confirmed PersistenceFailed,
   ValidationFailed, and RevisionExhausted diagnostics survive automatic
   refresh until the next explicit write; uncertain writes retain the last
   confirmed value and are never replayed. Both production QML surfaces expose
   the stable diagnostic.

The original six P1 and two P2 repair classes cleared by Talia North's exact
`55105b2` review remain intact.

## Verification on exact candidate source

- `cmake --build build/ada-debug -j2` — exit 0.
- `ctest --test-dir build/ada-debug -R '^qindaqt\.' --output-on-failure -j2`
  — exit 0, **69/69 passed**, zero failures.
- `cmake --build build/ada-release -j2` — exit 0.
- `ctest --test-dir build/ada-release -R '^qindaqt\.' --output-on-failure -j2`
  — exit 0, **69/69 passed**, zero failures.
- Focused canonical/protocol/service/client matrix — **8/8 passed**; split
  client/DND/private-transport slice — **3/3 passed**; settings/shell offscreen
  slice — **2/2 passed**.
- `cmake --build build/ada-production-release -j2` — exit 0.
- `cmake --build build/ada-production-release -j2 --target all_qmllint` —
  exit 0; only pre-existing unrelated preview-QML warnings were emitted.
- `tools/validate-docs` — exit 0, **42 documents**.
- `build/ada-mkdocs-venv/bin/mkdocs build --strict` — exit 0.
- `tools/check-source-shape --largest 30` — exit 0, **767 source files**, zero
  skips or violations. SettingsClient is 490 nonblank lines; the repaired
  decoder is 423 and its envelope/encoder collaborators are 132/141.
- `git diff --check`, cached diff check, and final commit diff check — exit 0.
- Production Release staged into isolated `build/ada-stage-prefix` — exit 0;
  service, activation descriptor, schemas, and QindaQt profile defaults are
  installed.
- Isolated `dbus-run-session` activation through the staged descriptor — exit
  0. `GetSnapshot` returned Applied, wire schema 1, settings schema 2, revision
  0, `appearance.animationDurationMs=int64(160)` from `profile-defaults`, and
  `services.doNotDisturb=false` from `system-defaults`; the service owned a
  private unique bus name.
- Final `git status --short --branch`: clean `worker/ada-settings1` at exact
  `08c7156c578eaac21116498ed563828be4c1a625`.

## Caveats and requested next action

No live desktop, real user session bus, compositor, assistive-technology
bridge, lock screen, KGlobalAccel, uinput, pointer, keyboard, main checkout,
reviewer checkout, or Mira worktree was used or modified. Private D-Bus and
offscreen QML remain the intentional safe boundary.

Please assign a different worker to re-review exact candidate
`08c7156c578eaac21116498ed563828be4c1a625` against both remaining P1 reports
(`1787849070` and `1787849341`/`1787849358`) plus the fixed-scalar marker,
direct-transport, canonical-Unicode/numeric, full-outcome, and startup-wire-fit
design checklist. Only an exact-hash review should decide integration.
