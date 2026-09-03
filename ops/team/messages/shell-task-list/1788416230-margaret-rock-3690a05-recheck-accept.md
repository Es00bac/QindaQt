# Margaret Rock — compositor shell window actions repair recheck

- Persona: Margaret Rock (`margaret-rock`), independent compositor/security reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `3690a056e667135d486da4fa60d7996882a4560a`
- Tree SHA: `ae70ed0f56ac200baa74bb68341916dc4bc2322a`
- Parent SHA: `ffa6cfef1b71f9c52f7431d6daaa7beb132d6b7a`
- Base SHA: `d0b70ed80d9c6bf45b9d3b6219d1e11514514c4c`
- Repaired product ancestor: `11f4c0a85851376623c34bfb8cfda2ddb5383bb3`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/compositor-window-actions-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex`

## Verdict

**ACCEPT. P0/P1/P2/P3 = 0/0/0/0.**

The exact repair descendant closes both prior findings. Authentication now
precedes semantic parsing, every raw field has an O(1) entry-length check before
any parse, unauthenticated/unbound/oversized replies are bounded and echo-free,
and the real private-bus client row proves owner replacement, old-owner late
reply rejection, no replay, and request timeout. No behavior or registry change
outside the bounded repair was introduced.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Prior-finding recheck

### P1-1 closed — authenticate and bound before semantic parsing

- `src/compositor/kwin/kwinshellwindowactions.cpp:222-230` no longer parses the
  revision. The endpoint obtains the D-Bus caller and forwards untouched raw
  strings to the policy controller.
- `src/compositor/src/shellwindowactions.cpp:147-188` first records only the
  three `QString::size()` values, then resolves the live panel PID, validates
  the caller's unique D-Bus name and exact credential PID, rate-admits, rejects
  a 64/128/20 overflow without echo, and only afterward calls `parseRevision()`
  and constructs the semantic generation.
- All unbound, non-unique, PID-mismatch, rate, and entry-bound exits use
  `rejectWithoutEcho()` (`src/compositor/src/shellwindowactions.cpp:42-47,
  151-184`). `encodeShellWindowActionResult()` omits window/generation fields
  when that fixed result is empty (`src/compositor/src/shellwindowactions.cpp:
  264-281`).
- The unit controls at `tests/compositor/tst_shellwindowactions.cpp:168-239`
  use megabyte-scale raw strings, assert zero generation/window lookup, and
  compare hostile and ordinary unauthenticated/unbound payloads exactly. The
  authenticated oversized control asserts the fixed entry-bound rejection.

Reviewer-only live reproduction, using only a private `dbus-run-session`,
disposable build-root XDG paths, cache-pinned KWin 6.6.5 virtual output, and no
host display, host bus, hardware, uinput, network, or `tests/session` row:

```sh
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/reviewer/run_review_live.py
```

Exit status `0`. Relevant observed stages:

```text
REVIEW_STAGE=bound-oversized-window-id:{"failure":{"code":"request-fields-too-large","message":"the shell action fields exceed their entry bounds"},"hasEpoch":false,"hasRevision":false,"hasWindowId":false,"replyBytes":157,"status":"control-disabled"}
REVIEW_STAGE=wrong-pid-hostile-size:{"echoedEpochCharacters":0,"failure":{"code":"caller-pid-mismatch","message":"the D-Bus caller does not own the shell panels"},"replyBytes":145,"status":"unauthorized"}
REVIEW_STAGE=unbound-hostile-size:{"echoedEpochCharacters":0,"failure":{"code":"shell-owner-unbound","message":"no single committed shell panel owner is bound"},"replyBytes":149,"status":"control-disabled"}
```

Observed versus expected: a bound caller's new one-megabyte window-ID shape was
rejected in 157 bytes with no request fields; the prior wrong-PID and unbound
megabyte shapes were rejected in 145 and 149 bytes with no request-field echo.
The same run also observed ordinary bound admission, stale-generation rejection,
fresh unminimize, and authority withdrawal after panel destruction.

### P2-1 closed — real-bus owner replacement and timeout evidence

`tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp:129-178`
uses separate old-service, replacement-service, and client bus connections. It
holds an exact-owner reply pending, transfers the well-known name, requires the
client to publish the replacement owner and finish the old operation as
`owner-changed`, asserts zero calls on the replacement, sends the old owner's
late reply, and asserts no second completion or replay. Lines 180-214 separately
hold a real D-Bus reply beyond the client deadline, require
`request-timeout`/uncertain, and verify the late reply neither completes again
nor causes another service call.

The row passed normally and for ten consecutive repetitions in both Debug and
Release. These are real Qt D-Bus watcher/pending-call paths on CTest's private
bus, not the fake transport unit.

## Documentation and scope audit

- ADR-0057, the compositor/session architecture page, the Compositor1 reference,
  and the testing harness now state the implemented order: O(1) raw lengths,
  live PID join, rate admission, entry-bound rejection, then generation/UUID
  parsing. They also state the compact no-echo failures and the client
  replacement/late-reply/timeout guarantees without expanding the accepted
  PID-join threat model.
- The repair commit changes only four owning wiki pages, three compositor
  public/implementation files, and three focused test files. It does not alter
  shell/task-list/launcher production composition, D-Bus descriptors, build
  registries, shared navigation, host integration, hardware, or network paths.
- Module direction remains compositor public values plus private KWin adapter,
  and the client still depends only on the public compositor action values plus
  Qt Core/DBus.

## Commands and results

All product commands ran from the exact candidate worktree. Reviewer scratch
was confined to the assigned build root.

### Provenance and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 11f4c0a85851376623c34bfb8cfda2ddb5383bb3^
git merge-base 11f4c0a85851376623c34bfb8cfda2ddb5383bb3 HEAD
git status --porcelain=v1
```

Results: worktree and SHAs exactly match the header; merge base is the repaired
ancestor `11f4c0a85851376623c34bfb8cfda2ddb5383bb3`; initial and final status
output was empty.

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

Results: Debug `0`; Release `0`. Both generated successfully with the existing
mixed-prefix runtime-path warnings.

### Build

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  --parallel 3 --target qindaqt_compositor qindaqt_shell_window_actions_tests \
  qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests \
  qindaqt_shell_window_actions_private_bus_tests tests/compositor/all
```

Results: both exited `0`; each rebuilt 77/77 required Ninja edges.

### Runtime tests

For each profile, run serially:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  -R '^compositor\.' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  -R '^qindaqt\.shell-window-actions-(client|private-bus)$' \
  --output-on-failure --no-tests=error
```

Results: Debug compositor 48/48 and client 2/2 passed; Release compositor 48/48
and client 2/2 passed. Every command exited `0`. The private virtual KWin row was
test 7/48 in each compositor selector.

Detailed new unit controls:

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile>/tests/compositor/qindaqt_shell_window_actions_tests -v1
```

Results: Debug 17/17 and Release 17/17 QtTest cases passed, including all three
new hostile-input controls; both exited `0`.

Private-bus stress, for each profile:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/<profile> \
  -R '^qindaqt\.shell-window-actions-private-bus$' --repeat until-fail:10 \
  --output-on-failure --no-tests=error
```

Results: ten consecutive executions passed in Debug and ten in Release; both
commands exited `0`.

### Static and documentation gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/site
./tools/check-source-shape
git diff --check
git diff HEAD^..HEAD --check
PYTHONPYCACHEPREFIX=/home/cabewse/work_SPaC3/builds/qindaqt/review-compositor-codex/pycache \
  python3 -m py_compile tests/compositor/test_shell_window_actions_nested.py \
  tests/compositor/test_dbus_contract.py
```

Results: all exited `0`; docs validated 119 Markdown documents and navigation;
MkDocs strict completed; source shape checked 1,829 files and reported only the
pre-existing 500-line compositor CMake and unrelated 539-line display-color test
warnings; both diff checks and Python syntax passed.

```sh
json_files=$(git diff --name-only HEAD^..HEAD -- '*.json'); \
  if [ -z "$json_files" ]; then echo '0 changed JSON files; json.tool not applicable'; \
  else for json_file in $json_files; do python3 -m json.tool "$json_file" >/dev/null || exit 1; done; fi
```

Result: exit `0`; the repair changes zero JSON files, so `json.tool` was not
applicable.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
