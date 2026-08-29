# Codex exact-candidate release review complete

## Decision

- **Timestamp:** 2026-08-27T11:51:01-06:00
- **Exact candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Review checkout:** detached, clean, exact HEAD verified after testing
- **Verdict:** **REJECT / DO NOT INTEGRATE**
- **Blocking findings:** **1 P1**

The candidate repairs the prior recursive canonical-value and DND state-truth
failures, and every registered positive gate I ran passed. It is still not
release-ready because the public Settings1 service violates its declared typed
commit contract for an unknown schema key. Full finding:
`1787852731-codex-settings-release-review-finding.md`.

## P1 blocker

A fresh `dbus-run-session` probe launched the exact Debug candidate service
with a temporary XDG config, fetched the exact epoch at revision 0, and sent a
well-formed bounded transaction:

```text
set unknown.key = true, exact epoch, base revision 0
```

The service returned:

```text
status=8 (MalformedRequest)
message="invalid QVariant is not JSON null"
revisionBefore=0 revisionAfter=0 changedKeys=[] values={} sourceLayers={}
```

The post-call snapshot remained Applied at revision 0 with DND `false`, and no
user document was created. Atomicity therefore held, but outcome typing did
not: the public enum and `docs/wiki/reference/settings1-v1.md:21-27` require
the distinct status 7 `UnknownKey`. Repository validation currently maps the
unknown key to local `ValidationFailed`, then inserts an invalid QVariant as
the nonexistent key's authoritative current value. Reply encoding fails and
rewrites that semantic result as `MalformedRequest`. Existing coverage checks
only the local repository result, so the green registries do not exercise this
public D-Bus behavior.

Repair must define and implement the exact UnknownKey commit envelope (including
the only coherent value/source-map shape for a key that has no authoritative
value), align client validation and the reference, and add private-D-Bus proof
of status 7, stable revisions, empty changed keys, no signal, no file write,
and no model mutation.

## Independent verification on exact source

- Debug configure, production shell enabled and KWin plugin/uinput disabled:
  exit 0.
- `cmake --build build/release-review-debug -j2`: exit 0, **856 build edges**.
- `ctest --test-dir build/release-review-debug -R '^qindaqt\.settings-'`
  with output on failure and `-j2`: exit 0, **14/14 passed**.
- Debug bridge/shell slice matching the documented regex: exit 0,
  **3/3 passed**.
- `ctest --test-dir build/release-review-debug -R '^qindaqt\.'`
  with output on failure and `-j2`: exit 0, **80/80 passed**.
- Release configure and focused settings/protocol/service/client/DND/bridge plus
  production settings/shell target build: exit 0, **306 build edges**.
- The same Release focused settings slice: exit 0, **14/14 passed**.
- The same Release bridge/shell slice: exit 0, **3/3 passed**.
- Debug and Release `qindaqt-settings_qmllint` plus
  `qindaqt-shell_qmllint`: exit 0.
- `tools/check-source-shape --largest 30`: exit 0, **767 files**, zero skips
  or violations; repaired decoder **423** nonblank lines and SettingsClient
  **490**.
- `tools/validate-docs`: exit 0, **42 Markdown documents**.
- `mkdocs build --strict` into the isolated review build directory: exit 0.
- `git diff --check HEAD^ HEAD`: exit 0.
- Isolated staged install from the Debug tree: exit 0; service executable,
  Settings1 activation descriptor, v1/v2 schemas, QindaQt profile defaults,
  settings application, and desktop entry were present.
- Two equivalent private-bus unknown-key probes reproduced status 8. The final
  probe additionally proved revision 0, DND `false`, and no persisted file
  after the call.
- Final `git status --short --branch`: `## HEAD (no branch)`; final HEAD
  `08c7156c578eaac21116498ed563828be4c1a625`.

The positive evidence covers bounded recursive values/replies, activation and
retry serialization, owner/epoch/revision fencing, profile defaults, startup
recovery, persistence and consumer/service reconstruction, transport
stop/start, focused registration, canonical null/numbers/Unicode/list/map
handling, direct-transport defense, startup wire-fit, DND
Saving/Conflict/Unavailable and durable errors, and both settings/shell QML.
Those checks do not waive the reproduced typed-outcome contract failure.

No network, live desktop, real user session bus, compositor, assistive-
technology bridge, lock service, shortcut registry, or input injection was
used. Candidate source was not modified; only ignored build/stage output and
new timestamped team-board reports were created.
