# Kay McNulty — Launcher L1 second-repair descendant recheck

- Persona: Kay McNulty, independent shell-applet reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `352cfd03db385fb497998e80ea2ad028bd744979`
- Tree SHA: `004fc8775bd094846bd26a8546e5bf31710e7342`
- Parent SHA: `9f45c5f9bd650ad1f693a1962f4992882c4272cf`
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Repaired product ancestor: `86a6d982325e2eaea90720713ac0d46dd86021b1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/launcher-l1-codex-review`
- Scratch/build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex`

The candidate is rejected with one P2. The host-isolation repair, dangling-link
diagnostic, ordinary unreadable-root diagnostic, and registered contract-text
controls work. However, an injected data root below a non-traversable parent is
still silently treated as absent rather than degraded. No product path was
edited; all scratch artifacts stayed under the assigned build root.

## Findings ledger

### P0

None.

### P1

None.

### P2

#### P2-1 — A data root hidden by a non-traversable parent is silently treated as normal absence

`src/shell/launcher/src/application_scanner.cpp:222-227` uses
`QFileInfo::exists()` plus `isSymLink()` as the first discriminator. When the
root physically exists but an ancestor denies traversal, both queries are false
because the metadata lookup fails with `EACCES`; line 227 then takes the normal
absent-root return without recording a scanner diagnostic. This contradicts
`docs/wiki/shell/launcher.md:124-128`, the public comment at
`src/shell/launcher/src/application_scanner.h:49-62`, and ADR-0056's requirement
that unreadable roots publish degraded truth.

I compiled `scanner_probe_r3` as a `QCoreApplication` against the exact
candidate's freshly built Debug runtime and pure-model archives. The fixture
creates a real `blocked/data-root/applications` tree, then removes traversal
permission only from `blocked`:

```text
$ fixture_root=/home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/repros/r3-inaccessible-ancestor
$ cmake -E remove_directory "$fixture_root"
$ mkdir -p "$fixture_root/blocked/data-root/applications"
$ chmod 000 "$fixture_root/blocked"
$ stat "$fixture_root/blocked/data-root"
stat: cannot statx '.../blocked/data-root': Permission denied
$ env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
    XDG_RUNTIME_DIR=.../repros/r3-isolated-debug \
    WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
    .../repros/scanner_probe_r3 "$fixture_root/blocked/data-root"
entry-count=0 first=[] diagnostics=0
[exit 0]
$ chmod 700 "$fixture_root/blocked"
```

Observed: the scanner publishes an empty, non-degraded catalog even though the
root lookup failed with permission denial. Expected: at least one bounded scan
diagnostic so the controller projects `Degraded`; only a confirmed nonexistent
root may be treated as normal absence. The registered regression at
`tests/shell/launcher/tst_application_scanner.cpp:161-192` chmods the root
itself while leaving its parent traversable, so it does not cover this inverse.
The workaround is to restore execute permission on every ancestor or restart
with a different root, making this bounded P2 rather than P1.

### P3

None.

## Recheck of the requested questions

1. **P0-1 host isolation is closed.** The runtime link interface at
   `src/shell/launcher/CMakeLists.txt:88-99` exposes Core/DBus but no
   Gui/QML/Quick/QuickControls2. The four previously affected tests use
   `QTEST_GUILESS_MAIN`. Compile commands define only `QT_CORE_LIB` and
   `QT_DBUS_LIB`; `ldd` shows Qt Core/DBus and no Qt Gui/QML/Quick dependency.
   `ctest -N -V` shows all 15 launcher registrations unset inherited session
   bus/display endpoints and use a private runtime directory; the two real GUI
   paths force offscreen software rendering internally.
2. **The named dangling-link and ordinary unreadable-root cases are repaired,
   but P2-1 is not fully closed.** The candidate scanner and controller
   selectors pass, and the scanner regression fails against `86a6d98` with
   zero diagnostics instead of two. The inaccessible-ancestor inverse above
   remains unhandled.
3. **The prior contract-text gap is closed and earlier functional closures
   remain green.** The new registered contract row passes on the candidate and
   rejects `40f1372` on all nine required/misleading text checks. The boundary
   control rejects `86a6d98` for its QML/Quick linkage and four non-guiless
   mains. The complete launcher selectors cover the previously repaired
   scanner FIFO/escape, persistence uncertainty, action inheritance, QML,
   accessibility, and relocation paths in both profiles.
4. **Required suites and gates otherwise pass.** Debug and Release each pass
   launcher 15/15 and applet integrity 3/3 under the hostile caller
   environment. Documentation, strict MkDocs, source shape, diff checks, and
   adjacent manifest JSON parsing pass. No nested session/compositor row, host
   D-Bus service, hardware/uinput path, network operation, or real application
   was invoked.

The repair commit changes its owning launcher implementation, focused tests,
and owning wiki/testing pages. Its parent adds coordination records only. The
repair range changes no shared registry or JSON; across the original lane base,
the launcher registry addition remains additive.

## Commands and results

### Identity and cleanliness

```text
$ git rev-parse HEAD
352cfd03db385fb497998e80ea2ad028bd744979
$ git rev-parse HEAD^{tree}
004fc8775bd094846bd26a8546e5bf31710e7342
$ git rev-parse HEAD^
9f45c5f9bd650ad1f693a1962f4992882c4272cf
$ git status --porcelain
[no output; exit 0 before review and before verdict write]
```

### Configure and focused builds

Both required configure commands exited 0; only the repository's existing
mixed-prefix runtime-search-path warnings were emitted:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
[exit 0]
```

For each profile, the following exact target list built with exit 0:

```text
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_shell_launcher qindaqt_shell_launcher_runtime \
  qindaqt_shell_launcher_qml qindaqt_shell_launcher_qmlplugin \
  qindaqt_desktop_entry_parser_tests qindaqt_application_catalog_tests \
  qindaqt_launcher_category_model_tests qindaqt_launcher_search_ranker_tests \
  qindaqt_launcher_pinned_recent_tests qindaqt_launcher_presentation_tests \
  qindaqt_launcher_scanner_tests qindaqt_launcher_execution_tests \
  qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests \
  qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests \
  qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests \
  qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
[Debug exit 0, 126/126 final actions; Release exit 0, 130/130 final actions]
```

Ninja reported `premature end of file; recovering` for the reused build roots'
logs, rebuilt the requested artifacts, and completed successfully. No product
file was involved.

### Hostile-environment selectors

Each command used the profile-specific private `XDG_RUNTIME_DIR` shown below:

```text
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/repros/r3-isolated-<profile> \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.launcher-' \
  --output-on-failure --no-tests=error
[Debug: exit 0, 15/15 passed; Release: exit 0, 15/15 passed]

env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY \
  XDG_RUNTIME_DIR=<ROOT>/repros/r3-isolated-<profile> \
  WAYLAND_DISPLAY=qindaqt-nonexistent QT_QPA_PLATFORM=wayland \
  ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.applet-(manifest|catalog|runtime-resolution)$' \
  --output-on-failure --no-tests=error
[Debug: exit 0, 3/3 passed; Release: exit 0, 3/3 passed]

$ ctest --test-dir <ROOT>/debug -N -V -R '^qindaqt\.launcher-'
[exit 0; 15 rows; every row unsets DBUS_SESSION_BUS_ADDRESS, DISPLAY, and
 WAYLAND_DISPLAY and sets the private runtime directory; GUI rows offscreen]
```

Selected repaired candidate rows:

```text
$ env <same hostile environment> .../qindaqt_launcher_scanner_tests \
    inaccessibleAndDanglingApplicationTreesDegrade
3 passed / 0 failed [exit 0]
$ env <same hostile environment> .../qindaqt_launcher_controller_tests \
    rootAccessFailuresReachTheProjection
3 passed / 0 failed [exit 0]
```

### Mutation sensitivity and linkage audit

```text
$ cmake -DSOURCE_ROOT=<extracted-86a6d98> \
    -P "$PWD/tests/shell/launcher/check_launcher_boundary.cmake"
[expected exit 1: Qt6::Qml and Qt6::Quick runtime leakage plus four missing
 QTEST_GUILESS_MAIN controls]

$ cmake --build <ancestor-86-standalone-build> --parallel 3 \
    --target qindaqt_launcher_scanner_tests
[exit 0]
$ env <hostile environment> <ancestor-86-standalone-build>/qindaqt_launcher_scanner_tests \
    inaccessibleAndDanglingApplicationTreesDegrade
Actual diagnostics: 0; expected: 2; 2 passed / 1 failed [expected exit 1]

$ cmake -DSOURCE_ROOT=<extracted-40f1372> \
    -P "$PWD/tests/shell/launcher/check_launcher_contract_text.cmake"
[expected exit 1; nine required/rejected contract-text checks fired]

$ for target in qindaqt_launcher_scanner_tests qindaqt_launcher_executor_tests \
    qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests; do \
    ninja -C <ROOT>/debug -t commands "$target" | rg -m1 'tst_.*\.cpp\.o'; done
[exit 0; QT_CORE_LIB and QT_DBUS_LIB only, no GUI/QML/Quick definitions]

$ for binary in qindaqt_launcher_scanner_tests qindaqt_launcher_executor_tests \
    qindaqt_launcher_persistence_tests qindaqt_launcher_controller_tests; do \
    ldd "<ROOT>/debug/tests/shell/launcher/$binary" | \
      rg 'libQt6(Core|DBus|Gui|Qml|Quick)'; done
[exit 0; only libQt6Core and libQt6DBus matched for every binary]
```

One optional scratch watcher-probe compile was aborted with exit 130 after an
incorrect compiler language-mode ordering caused archive inputs to be parsed as
source. It produced no evidence and changed no product path.

### Static gates

```text
$ ./tools/validate-docs
Validated 117 Markdown documents and mkdocs.yml navigation. [exit 0]

$ /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
    --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-launcher-codex/site
Documentation built in 1.28 seconds. [exit 0]

$ ./tools/check-source-shape
Checked 1784 source files; skipped 0; no violations. [exit 0; two existing
decomposition-review advisories]

$ git diff --check
[no output; exit 0]
$ git diff --check ce9228d9694622d503d92a38d01986f8f124f188..HEAD
[no output; exit 0]
$ git diff --name-only ce9228d9694622d503d92a38d01986f8f124f188..HEAD | rg '\.json$'
[no output: no JSON changed]
$ python3 -m json.tool data/applets/launcher.json
[formatted JSON; exit 0, adjacent unchanged manifest check]
```

## Verdict

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
