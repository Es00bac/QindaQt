# Sophie Wilson — Clipboard C1 candidate handoff

- Candidate commit: `405577cc964dd1282a9210af282650a132391056`
- Candidate tree: `09cedd7da0d052b980deeb4e1a1f6ef19c71b6e7`
- Exact base: `ce9228d9694622d503d92a38d01986f8f124f188`
- Branch: `worker/clipboard-service-c1`
- Requested next action: independent exact review, then manager integration.

## Changed paths

- `docs/wiki/adr/0056-isolate-clipboard-capture-in-a-volatile-host.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/clipboard-service.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/clipboard1-v1.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/services/clipboard_client/**`
- `src/services/clipboard_protocol/**`
- `src/services/clipboard_service/**`
- `src/services/clipboard_wayland_adapter/**`
- `tests/CMakeLists.txt`
- `tests/services/clipboard_client/**`
- `tests/services/clipboard_protocol/**`
- `tests/services/clipboard_service/**`
- `tests/services/clipboard_wayland_adapter/**`

## Acceptance evidence

All commands ran from the candidate worktree unless a build directory is named.

- Exact brief Debug configure command with build directory
  `/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/debug`: exit 0.
- `cmake --build .../debug --parallel 3 --target qindaqt_clipboard_protocol qindaqt_clipboard_client qindaqt_clipboard_wayland_adapter qindaqt_clipboard_service qindaqt-clipboard-host qindaqt_clipboard_protocol_tests qindaqt_clipboard_client_tests qindaqt_clipboard_wayland_adapter_tests qindaqt_clipboard_service_tests qindaqt_clipboard_private_bus_tests qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests`: exit 0.
- `ctest --test-dir .../debug -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error`: exit 0, 12/12 passed. The final affected private-bus and staged-install reruns also passed 1/1 each after their last edits.
- Exact brief Release configure command with build directory
  `/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/release`: exit 0.
- The same focused target build under `.../release`: exit 0.
- `ctest --test-dir .../release -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error`: exit 0, 12/12 passed. The final affected private-bus and staged-install reruns also passed 1/1 each after their last edits.
- `./tools/validate-docs`: exit 0, 118 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-service-c1/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 1802 files checked; only two pre-existing out-of-lane decomposition warnings were reported.
- `git diff --check`: exit 0.
- No JSON was changed, so the JSON formatting gate was not applicable.

## Bounded caveats

- No host Wayland display, host session bus, nested compositor, hardware,
  uinput, network, or systemd user manager was touched. Fake generated-protocol
  and private-bus behavior does not claim packaged KWin interoperability or a
  live activation transaction.
- The accepted plan deliberately has no `wlr-data-control` fallback; absence of
  `ext-data-control-v1` reports unavailable.
- Both pre-existing Settings schemas declare `services.clipboardHistory`
  default `true`. Lane ownership allowed schema edits only if the key was
  absent, so the candidate leaves those files untouched. The host itself starts
  disabled and accepts only confirmed Boolean Settings1 truth; changing the
  schema-level product default remains a manager/schema-owner integration task.
- The shell applet, UI composition, persistence, synchronization, and payload
  search are excluded from this candidate.
