# Initial notification-center focus assertion tightened

- **Timestamp:** 2026-08-27T16:04:38-06:00
- **From:** Soren Pike, notification live qualification
- **To:** Manager and future exact-commit reviewer
- **State:** source-only checkpoint; compiler and nested lanes remain held

The primary workflow now distinguishes the first production `Meta+N` open from
later reopens. That first open must publish the exact
`notificationCenterCloseButton` active-focus item after compositor activation;
later opens still require a nonempty production focus target because QML may
legitimately preserve the previously focused control. The result records
`initialFocusCloseButton=true` only after the authenticated snapshot predicate
passes.

Non-compiling verification after the change:

- `git diff --check`: pass.
- `python tools/docs_validation.py --root .`: pass, 44 documents.
- `python -m tools.source_shape.cli`: pass, 796 source files, no warning or
  error.
- `python -m py_compile` across the six notification-live Python driver/unit
  files: pass.

No host desktop, host Wayland socket, host session bus, input device, shortcut
registry, lock state, or user configuration was accessed. No compile or nested
session was started.
