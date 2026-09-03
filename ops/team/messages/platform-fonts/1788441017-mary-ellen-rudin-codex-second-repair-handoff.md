# Mary Ellen Rudin-Codex — Font F1 second repair handoff

- Timestamp: 2026-09-03T07:10:17-06:00
- Candidate commit: `4f5452f2e4792fddc724ec4331e9591aa98567f1`
- Candidate tree: `5e788753140b9cb5f79823aff3a1e2d1a71dddcb`
- Candidate parent: `c536dc0d9e86c4534822c983e7e4e6f8e4a5025a`
- Rejected ancestor: `84367aafe16a410fc51e39fda430abaafdcc39d6`
- Exact lane base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Branch: `worker/font-discovery-f1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/font-discovery-f1`

## Outcome

The blocking pre-application reader now re-resolves the Settings1 well-known
name after its unique-name `GetSnapshot` call and before accepting the reply,
inside the original 750 ms total budget. Owner loss, replacement, malformed
owner replies, and budget exhaustion all fail closed. `FontSettingsBridge`
now clears its public write-baseline authority on every non-Ready client state
while preserving coordinator LKG preferences. The testing-harness page no
longer claims that `/dev/null` exercises `productionDefault()`; it identifies
that poison as belonging only to the injected-directory rejection row.

## Changed product paths

- `docs/wiki/architecture/font-preferences.md`
- `docs/wiki/development/testing-harness.md`
- `src/services/font_discovery/include/qindaqt/services/font_discovery/font_session_bootstrap.h`
- `src/services/font_discovery/src/font_session_bootstrap.cpp`
- `src/services/font_preferences/src/font_settings_bridge.cpp`
- `tests/services/font_discovery/font_session_bootstrap_test_support.h`
- `tests/services/font_discovery/tst_font_session_bootstrap.cpp`
- `tests/services/font_preferences/tst_font_settings_bridge.cpp`

## Reproduction and discriminating controls

Before the repair, Cecilia's exact rebuilt reproducers against `84367aa` ran
with exit 0 and printed:

```text
probe_exit=0 owner_after_reply=0 applied=1 family=Liberation Mono size=13.5
owner loss baseline truth: before=1 state=0 after=1
```

The review source/prose reproduction also exited 0:

```sh
rg -n 'productionDefault|FONTCONFIG_FILE|/dev/null' \
  tests/services/font_discovery -g '*.cpp' -g '*.cmake' -g '*.h'
```

It confirmed the only production-default discovery call carries a staged
`FONTCONFIG_FILE`, while `/dev/null` occurs only in the injected-directory
rejection row. The corrected prose now states exactly that scope.

For ancestor-negative evidence, a detached exact-`84367aa` source under the
assigned build root received only the new test/test-support diff. Its strict
Debug test targets built with exit 0, then this registered selector exited 8
as expected with 0/2 CTest rows passing:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  FONTCONFIG_FILE=/dev/null FONTCONFIG_PATH=/nonexistent \
  HOME=<ROOT>/ancestor-84367aa-home TMPDIR=<ROOT>/ancestor-84367aa-tmp \
  ctest --test-dir <ROOT>/ancestor-84367aa-build \
  -R '^qindaqt\.font-(session-bootstrap|settings-bridge)$' \
  --output-on-failure --no-tests=error
```

`FontSessionBootstrapTests::probeRejectsSnapshotAfterOwnerLoss()` failed at
`!outcome.applied`; `FontSettingsBridgeTests::transportLossFailsClosed()`
failed at `!bridge.hasBaseline()`. Each registered row had exactly one failed
assertion, demonstrating both regressions on the rejected implementation.

## Configure and build evidence

Debug and Release were configured from this worktree with the lane's exact
recipe (private KWin 6.6.5 cache, testing/plugin/shell/production shell on,
host uinput off, strict warnings on). Both configure commands exited 0.

The following focused build exited 0 in both Debug and Release:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_font_discovery qindaqt_font_preferences \
  qindaqt_font_discovery_tests qindaqt_font_discovery_hostile_tests \
  qindaqt_font_discovery_bounds_tests qindaqt_font_session_bootstrap_tests \
  qindaqt_font_catalog_tests qindaqt_font_preferences_tests \
  qindaqt_font_preferences_codec_tests qindaqt_font_bootstrap_tests \
  qindaqt_font_preferences_coordinator_tests qindaqt_font_settings_bridge_tests \
  qindaqt_font_settings_bootstrap_tests qindaqt-editor qindaqt-file-manager \
  qindaqt-terminal qindaqt-settings
```

An initial development build of the new test fixture exited 1 because the
QDBus virtual-object callback exposes a const connection; the fixture was
corrected to retain its own connection, after which the same build exited 0 in
both profiles.

## Required sanitized test evidence

Every row below used exactly the requested ambient poison plus a build-root
temporary directory:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  FONTCONFIG_FILE=/dev/null FONTCONFIG_PATH=/nonexistent \
  HOME=<ROOT>/<profile>/home TMPDIR=<ROOT>/<profile>/test-tmp \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.font-' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 16/16 passed, 38.06 s.
- Release: exit 0, 16/16 passed, 37.03 s.

The four installed application rows ran with the same environment:

```sh
ctest --test-dir <ROOT>/<profile> \
  -R '^(qindaqt\.(settings-app-installed-routes|editor-installed-theme-and-metadata|file-manager-installed-runtime|terminal-installed-metadata))$' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 4/4 passed, 15.86 s.
- Release: exit 0, 4/4 passed, 14.33 s.

## Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir <ROOT>/site
./tools/check-source-shape
git diff --check
python3 -m json.tool <each changed JSON>
```

All applicable gates exited 0. `validate-docs` validated 118 Markdown documents
and navigation; strict MkDocs completed; source-shape checked 1,812 files with
only the two existing 500/539-line decomposition-review warnings; diff checks
passed. No JSON changed, so the JSON gate is vacuous. An optional local
`clang-format --dry-run --Werror` invocation exited 1 because the installed
formatter proposes whole-file changes to the pre-existing repository style;
it is not a repository gate and no bulk formatting was applied.

## Bounded caveats

- This candidate deliberately makes no claim about a host session bus, host
  font installation changes, a nested compositor, hardware, uinput, network,
  or rendered typography baselines.
- No `tests/session` row, host D-Bus service, hardware/uinput test, network
  call, or default whole-repository build/test suite was run.
- `/dev/null` remains ambient poison for the sanitized suite and the injected
  ill-formed-request row; it is deliberately not claimed as a dedicated
  `productionDefault()` proof.
- The Font1 resident-service, live catalog refresh, global monospace default,
  and logical-DPI application boundaries remain future work.

Requested next action: independent exact review by Cecilia Berdichevsky, then
Program Manager integration if accepted.
