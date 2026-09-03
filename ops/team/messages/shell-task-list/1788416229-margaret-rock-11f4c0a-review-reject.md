# Margaret Rock — independent compositor/security exact-candidate review

- Persona: Margaret Rock (`margaret-rock`), independent compositor/security reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `11f4c0a85851376623c34bfb8cfda2ddb5383bb3`
- Tree SHA: `7a3e9b96126ba1bbe9c11b5f5113c20f47e56e85`
- Sole parent SHA: `d0b70ed80d9c6bf45b9d3b6219d1e11514514c4c`
- Base SHA: `d0b70ed80d9c6bf45b9d3b6219d1e11514514c4c`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/compositor-window-actions-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex`

## Verdict

**REJECT. P0/P1/P2/P3 = 0/1/1/0.**

The ordinary-window behavior and the principal PID/generation fences work in the private nested runtime, but the public mutation endpoint violates its documented authenticate-before-generation contract and has no hostile argument/reply bound. The required real-private-bus owner-replacement/no-replay negative control is also absent.

## Findings ledger

### P0

None.

### P1

#### P1-1 — An unauthenticated peer can force unbounded generation parsing and reflected replies before the PID gate

The endpoint calls `strictRevision(revision)` before it obtains and authenticates the D-Bus caller at `src/compositor/kwin/kwinshellwindowactions.cpp:237-246`. The controller does not inspect the panel owner or caller credentials until `src/compositor/src/shellwindowactions.cpp:128-149`, and its rate limiter is later still at lines 150-153. Rejections preserve attacker-controlled `windowId` and `epoch`, and `encodeShellWindowActionResult()` copies both into JSON without an endpoint bound at `src/compositor/src/shellwindowactions.cpp:227-241`.

This contradicts the normative statement that every call is authenticated before generation lookup/parsing at `docs/wiki/reference/compositor-control-v1.md:221-225` and `docs/wiki/adr/0057-authenticate-shell-window-actions-by-panel-owner.md:52-56`. It also leaves wrong-PID and unbound callers outside the claimed action-rate bound while they consume the compositor GUI thread and allocate a reflected response up to the session bus message ceiling.

Reproduction (reviewer-only source and runtime live under the assigned build root; no product path was edited):

```sh
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/reviewer/run_review_live.py
```

Exit status: `0`. Relevant observations from private `dbus-run-session` plus virtual KWin 6.6.5:

```text
REVIEW_STAGE=wrong-pid-hostile-size:{"echoedEpochCharacters":1048576,"echoedRevision":"0","failure":{"code":"caller-pid-mismatch","message":"the D-Bus caller does not own the shell panels"},"replyBytes":1048797,"status":"unauthorized"}
REVIEW_STAGE=unbound-hostile-size:{"echoedEpochCharacters":1048576,"echoedRevision":"0","failure":{"code":"shell-owner-unbound","message":"no single committed shell panel owner is bound"},"replyBytes":1048801,"status":"control-disabled"}
```

Observed: a separate wrong-PID process supplied a 1,048,576-character epoch and a 1,048,576-digit revision while a valid panel was bound. The endpoint semantically parsed the revision to `0` before authentication and returned a 1,048,797-byte reply reflecting the epoch. With no panel, the same request returned 1,048,801 bytes. Neither path is rate limited.

Expected: authentication must precede semantic generation parsing as documented, hostile fields must have explicit entry bounds, and unauthenticated/unbound failures must be constant-sized or otherwise bounded. A wrong-PID peer must not be able to make the compositor scan and reflect arbitrary D-Bus-sized strings outside the request limiter.

### P2

#### P2-1 — The private-bus client test omits the required owner-replacement/no-replay negative control

`tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp:46-71` registers one fake compositor on the test process's single session-bus connection, sends one successful action, then unregisters only after completion. It never replaces the well-known owner while an action is pending, never lets an old unique owner reply late, and never proves that no request is replayed to the replacement. The owner-change and failure cases at `tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp:115-183` use a fake transport and therefore do not exercise the Qt D-Bus service watcher, asynchronous `GetNameOwner` race, or unique-owner delivery.

Reproduction:

```sh
rg -n 'unregisterService|registerService|serviceOwnerChanged|owner-changed|request-timeout|requestFailed' \
  tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp \
  tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/debug \
  -R '^qindaqt\.shell-window-actions-(client|private-bus)$' \
  --output-on-failure --no-tests=error
```

Exit statuses: `0`, `0`; the selector passes `2/2`. Observed: replacement/timeout assertions occur only in the fake-transport unit; the real private-bus file contains one registration and one final unregistration, with no replacement or pending-call case. Expected: ADR-0057 lines 79-82 explicitly require private-bus exact-owner replacement and no-replay coverage. This is a missing negative control at the transport boundary, not a product failure inferred from the passing fake.

### P3

None.

## Review-question evidence

### 1. Authentication

- Admission is `KWinShellPanelOwnerSource::shellPanelProcessId()` followed by session-bus `GetConnectionCredentials` `ProcessID` equality. The owner source scans tracked layer-shell objects on every call, accepts only committed exact `scope=dock` surfaces whose clients are not tearing down, and requires one positive PID (`src/compositor/kwin/kwinshellwindowactions.cpp:73-123`). The controller then requires a D-Bus unique name and exact credential PID equality (`src/compositor/src/shellwindowactions.cpp:131-149`).
- The private nested candidate row proved a separate wrong-PID process receives `unauthorized`. The reviewer probe independently observed `before-panel=control-disabled`, a bound primary caller `admitted`, and `post-panel-destroy=control-disabled`; surface destruction therefore clears ordinary authority.
- A second physical D-Bus connection from the panel-owning process has a different unique name but the same credential PID. The reviewer probe observed `same-pid-second-unique=admitted`. That follows the implemented/ADR process-PID authority; the server does not bind one shell bus unique name. Rate accounting is consequently per connection, as documented, not per PID.
- Two live processes cannot simultaneously have the same kernel PID. After shell exit, PID reuse is possible; the source's `client->tearingDown()` check and `aboutToBeDestroyed` removal close the ordinary path once KWin observes disconnect. The design has no pidfd or previously bound bus-name identity to eliminate a theoretical event-delivery interval between process death and Wayland teardown. I could not force exact PID reuse in the isolated runtime, so I do not promote that residual source-level concern to P0-P2. ADR-0057 correctly says PID equality is not a durable identity, though a repaired threat model should spell out this teardown ordering.
- The new object is intentionally live in production `read-only` mode for authenticated shell actions. It does not consult or enable `Compositor1` development mutation mode. P1-1 shows that unauthenticated calls still receive semantically processed/reflected inputs before the credential decision.

### 2. Fencing and bounds

- `currentGeneration()` synchronously refreshes the retained shell visibility sample and requires nonzero revision before comparison (`src/compositor/kwin/kwinshellwindowactions.cpp:125-149`). Controller order is generation before UUID lookup; the focused fake test verifies a stale generation performs zero registry lookups.
- The reviewer nested probe observed a post-minimize replay of revision `2` return `stale`, while fresh revision `3` admitted unminimize. Unknown UUID, rate reset/limit, and typed codec behavior passed in `compositor.shell-window-actions`.
- The client targets the exact compositor unique owner, allows one in-flight operation, matches owner/action/UUID/epoch/revision, and makes timeout/owner/transport/malformed outcomes uncertain without retry. The focused client unit passed, subject to P2-1's missing real-bus replacement case.
- Independent close calls `KWin::Window::closeWindow()`; Hybrid close enters the existing confirmation prompt and later calls `closeWindow()` on copied member IDs. No kill/process termination path was introduced (`src/compositor/kwin/kwinshellwindowactions.cpp:178-195`, `src/compositor/kwin/kwinhybridwindowactions.cpp:378-431`).
- Hybrid targets retain their container ID and are revalidated against both topology and registry before group activate/raise, whole-group minimize/unminimize, or close-confirmation policy. The ordinary live row does not exercise a live Hybrid group, but the focused policy seam and adjacent Hybrid suite passed.
- Hostile field bounds fail as detailed in P1-1.

### 3. Production read-only rule

- The candidate does not change the `Compositor1` descriptor or mutation gate. The new object path is `/org/qindaqt/CompositorShell`, has only five window actions, and exposes no docking, topology, output, test-input, or compositor-lifecycle method.
- The candidate diff adds the separate object unconditionally but never sets `m_mutationsEnabled`; legacy `Compositor1` production mutators remain on their unchanged pre-parse `control-disabled` path. No bypass to `DockWindows`, `Submit`, `ReleaseContainer`, or output mutation was found.
- Per the brief, no `tests/session` nested-compositor row was run. The unchanged legacy gate was verified by source/diff inspection; the requested `^compositor\.` selector in this configuration contains 48 tests and does not register the separate `tests/session` production row.

### 4. Tests

- Fake collaborator unit: present and passing (`compositor.shell-window-actions`), covering all five actions, wrong PID, unbound owner, stale-before-lookup, unknown UUID, Hybrid target preservation, rate limit, executor rejection, and codec failure.
- XML contract: present and passing (`compositor.dbus-contract`), with exact five-method/signature/no-signal checks for `org.qindaqt.CompositorShell1`.
- Client tests: fake-transport behavior and one real private-bus happy path pass; P2-1 records the absent private-bus replacement/no-replay control.
- Nested row: run serially through the requested `^compositor\.` command in each profile; it passed and observed activate, minimize, unminimize, raise order, request-close disappearance, and wrong-PID denial on a private bus/virtual KWin.
- Reviewer nested extension additionally proved live pre-bind rejection, ordinary unbind revocation, same-PID/different-unique-name behavior, and stale-generation rejection. It also produced P1-1.

### 5. Boundaries and documentation

- No `src/shell/runtime`, `src/shell/task_list`, or `src/shell/launcher` path changed.
- Shared registry/build/navigation edits are additive: one `src` subdirectory, one test subdirectory/include, and one ADR navigation/index row.
- `check-source-shape` passed. It reported only the existing 500-line `tests/compositor/CMakeLists.txt` and unrelated 539-line display-color test warnings; candidate additions did not enlarge either.
- Module direction is preserved: the shell client consumes the public compositor action values plus Qt Core/DBus; KWin types stay inside the compositor adapter.
- ADR-0057 acknowledges that PID is not durable and does not protect a compromised shell or an impersonated dock client. Its claim that authentication precedes generation handling is false because of P1-1. Its required private-bus evidence is not met because of P2-1.

## Commands and results

All commands below ran from the exact candidate worktree unless an absolute reviewer-scratch path is shown.

### Provenance and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git status --porcelain=v1
```

Initial and final results: expected worktree path; candidate/tree/parent exactly as listed in the header; status output empty. No product file was edited, committed, amended, or rebased.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Results: Debug `0`; Release `0`. CMake emitted the repository's existing mixed-prefix runtime-path warnings; generation completed.

### Build

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  --parallel 3 --target qindaqt_compositor qindaqt_shell_window_actions_tests \
  qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests \
  qindaqt_shell_window_actions_private_bus_tests
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  --parallel 3 --target tests/compositor/all \
  qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests
```

Results: all four commands exited `0`; focused builds completed `173/173` Ninja edges in each profile and adjacent compositor builds completed `261/261` in each profile.

### Runtime tests

For each of `debug` and `release`, serially:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  -R '^compositor\.' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  -R '^qindaqt\.shell-window-actions-(client|private-bus)$' \
  --output-on-failure --no-tests=error
```

Results: Debug compositor `48/48` passed, client `2/2` passed; Release compositor `48/48` passed, client `2/2` passed. Each command exited `0`. The nested `compositor.kwin-shell-window-actions` row ran as test 7/48 and passed in both profiles.

Reviewer-only private nested extension:

```sh
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/reviewer/run_review_live.py
```

Result: exit `0`; stage outcomes are recorded under P1-1 and review question 1. It used a private `dbus-run-session`, disposable build-root XDG paths, no inherited display/Wayland/session-bus address, virtual KWin, no lock screen, and no global shortcuts.

Python syntax:

```sh
PYTHONPYCACHEPREFIX=/home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/pycache \
  python3 -m py_compile tests/compositor/test_shell_window_actions_nested.py \
  tests/compositor/test_dbus_contract.py
```

Result: exit `0`.

### Static/documentation gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/site
./tools/check-source-shape
git diff --check
git diff HEAD^..HEAD --check
files=$(git diff --name-only HEAD^..HEAD -- '*.json'); \
  if [ -z "$files" ]; then echo '0 changed JSON files; gate not applicable'; \
  else for file in $files; do python3 -m json.tool "$file" >/dev/null || exit 1; done; fi
```

Results: all exited `0`; docs validated 119 Markdown documents plus navigation; MkDocs strict build completed; source shape checked 1,829 files; both diff checks were clean; candidate changed zero JSON files, so `json.tool` was not applicable.

## Final verdict

VERDICT REJECT P0/P1/P2/P3=0/1/1/0
