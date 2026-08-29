# Final release review: REJECT exact Settings1 candidate `2a1e262`

- **Timestamp:** 2026-08-27T12:42:05-06:00
- **Reviewer:** Codex Settings Final Release Reviewer
- **Exact candidate:** `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Original base:** `496e5135ee4f40359f8b871eec130f0b8b02a241`
- **Verdict:** **REJECT — one blocking P2 remains**

## Scope and tree identity

I reviewed the cumulative 103-file, 9,220-insertion/273-deletion candidate from
the original base through all four commits, not only the final diff:

1. `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
2. `55105b2c565f25f0582303e4936bcd288b04ffdb`
3. `08c7156c578eaac21116498ed563828be4c1a625`
4. `2a1e2626e5d4e8e4526bfadbb8100931208f3179`

The detached reviewer worktree was clean at the initial checkpoint and remains
exactly at candidate HEAD with `## HEAD (no branch)`, no tracked/cached diff,
and successful working/cached whitespace checks. I edited no product file and
created no commit.

## Blocking finding

`1787855945-codex-settings-final-release-review-finding.md` records the P2:
the prefix-staged, D-Bus-activated `qindaqt-settings-service` reproducibly
survives permanent private session-bus loss with a dead `QDBusConnection` and
no exit/reconnect policy. The final controlled run returned a successful
snapshot and `dbus-run-session` status 0; one second after the daemon ended,
exact staged PID 3274080 remained alive and retained the dead private bus
address. `main.cpp:48-57` enters the event loop after a one-time start check;
`resident_settings_service.cpp:87-101,219-222` neither observes disconnect nor
includes connection health in running state. The existing lifecycle test stops
service objects before ending the daemon and does not cover the installed
resident process across bus loss (`tst_settings_service_lifecycle.cpp:238-250`).

Every exact fixture PID was terminated. A final anchored process query confirms
that no staged Settings1 service remains.

## Repaired UnknownKey boundary evidence

The repaired UnknownKey behavior itself passes all three required boundaries:

- Repository verbose tests: Debug 6/6 and Release 6/6. Unknown set/remove,
  stale-base precedence, and revision-exhaustion precedence return UnknownKey
  with stable revision and empty authority.
- Real private-D-Bus lifecycle tests: Debug 4/4 and Release 4/4. Set and remove
  return exact wire ordinal 7, stable revision, empty values/sources/changed,
  no signal, no file, and no model mutation.
- Public client-validator verbose tests: Debug 7/7 and Release 7/7. Exact empty
  set/remove replies are accepted; value-only, source-only, and fabricated-pair
  UnknownKey replies are rejected as uncertain, trigger authoritative resync,
  and are not replayed.

The independent installed activation probe also passed. From a new XDG config
on a new `dbus-run-session`, the staged descriptor activated the exact staged
binary. Unknown set, remove, mixed known/unknown, and unknown with stale base 99
all returned status 7 with `revisionBefore == revisionAfter == 0` and empty
changed/value/source maps. The mixed request did not partially set DND. A wrong
epoch plus unknown key returned EpochMismatch status 6, proving envelope/epoch
precedence. Before/after snapshots stayed at revision 0 with DND false, the
signal count was zero, no user file existed, and explicit PID teardown passed.
The complete output is in
`build/settings-final-review-runtime/installed-probe.log`.

## Commands and gate results

All build directories were reviewer-owned and absent before configuration.

- Fresh Debug configure and build with shared libraries, testing, shell and
  production shell on, KWin plugin and host-uinput tests off, and strict
  warnings on: **0**, 860/860 build edges.
- Same fresh Release configure/build: **0**, 860/860 build edges.
- Fresh production Release configure with `BUILD_TESTING=OFF`, then build:
  **0**, 429/429 build edges.
- Corrected quoted selections using `ctest -N -R '^qindaqt[.]settings-'`:
  **15 selected in Debug and 15 in Release**.
- Focused runs with `--no-tests=error --output-on-failure -j2`: Debug **15/15**,
  Release **15/15**, status 0.
- Corrected full selections using `ctest -N -R '^qindaqt[.]'`: **81 selected in
  Debug and 81 in Release**.
- Full runs with `--no-tests=error --output-on-failure -j2`: Debug **81/81**,
  Release **81/81**, status 0. Selection/result logs are
  `build/settings-final-review-{debug,release}/review-{focused,full}-*.log`.
- `all_qmllint` in Debug, Release, production, and prefix-staged production
  builds: **0** in all four. It emits the established non-fatal shell-preview
  warnings outside this candidate diff; the changed Settings UI lint target is
  clean.
- `tools/check-source-shape --warnings-as-errors --largest 30`: **0**, 768
  checked, 0 skipped; largest affected production sources remain below the
  500-line review threshold (`settings_client.cpp` 490,
  `settings_wire_decode.cpp` 423, `shellruntimeapplication.cpp` 472).
- `tools/validate-docs`: **0**, 42 Markdown documents/navigation validated.
- `uvx --offline --from mkdocs mkdocs build --strict`: **0**. Standalone
  `mkdocs` is not installed, so the repository-established offline runner was
  used.
- Cumulative `git diff --check`, `git show --check`, working/cached whitespace,
  and final clean-tree checks: **0**.
- Fresh prefix-aware production configure/build/install with an absolute
  reviewer stage prefix: **0**. The stage contains 159 files, including v1/v2
  schemas, profile defaults, public headers, settings service/app/shell
  binaries, desktop entry, and `org.qindaqt.Settings1.service`; its `Exec=`
  names the exact staged binary.
- Isolated installed private-D-Bus activation/UnknownKey probe: **0**, `PROBE=PASS`.
- Controlled installed bus-loss lifetime reproduction: command **0**, defect
  reproduced, exact fixture cleanup **PASS**.

The focused/full registries also regress bounded recursive pre-allocation D-Bus
decoding; canonical null/wide signed/unsigned/double/Unicode save/restart
fidelity; exact owner/epoch/revision generation fencing; no uncertain replay;
startup rollback/release and explicit restart; DND Loading/Ready/Saving/
Conflict/Unavailable truth; lock privacy precedence; accessibility and route
boundaries; and shell/client dependency direction. Static cumulative review
found no additional P1/P2/P3 and found the wiki/reference/ADR text accurate for
the repaired UnknownKey contract. The resident bus-loss statement/coverage is
the one lifecycle exception described above.

No live desktop, user session bus, compositor, KGlobalAccel, input injection,
cursor, or lock action was used or claimed.

## Requested manager action

Do **not** integrate `2a1e2626e5d4e8e4526bfadbb8100931208f3179`.
Return the P2 to the implementer for a new exact commit that gives the installed
resident service deterministic permanent-bus-loss behavior (normally prompt
exit so replacement activation obtains a fresh process/owner/epoch) and adds a
process-level private-daemon-loss regression. Then assign this reviewer to
recheck that exact repaired commit; passing prose or the current test registry
cannot supersede the observed process evidence.
