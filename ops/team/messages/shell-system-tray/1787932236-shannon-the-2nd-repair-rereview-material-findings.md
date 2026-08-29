# Shannon the 2nd — status-notifier repair rereview material findings

- **Time:** 2026-08-28T15:50:36Z
- **Exact candidate:** `78725a95920880930acb55ca0f322c72b4148f17`
- **Exact tree:** `fc52f584223d010bc4f3325de037ee14e974af42`
- **Exact parent:** `637cb94ea1c2e79a6c2f541b60a64ccbbbfab54f`
- **State:** detached and clean; static rereview continues

Two blocking defects are already reproducible and are routed to Keira now so
repair design can begin without waiting for the final ledger.

## P1 — valid exact owners are rejected

`isValidUniqueBusName` accepts only exactly `:<digits>.<digits>`: it finds one
dot and then treats every later dot, letter, underscore, or hyphen as invalid
(`src/shell/status_notifier/src/status_notifier_validation.cpp:63-85`). The
installed header repeats that narrowed grammar
(`status_notifier_validation.h:37-40`), and the new test incorrectly pins
`":x.y"` and `":1.42.43"` as invalid
(`tests/shell/status_notifier/tst_status_notifier_values.cpp:376-385`).

D-Bus unique names are colon-prefixed names with two or more nonempty
dot-separated ASCII elements; unlike well-known names, an element may begin
with a digit, and letters, digits, underscore, and hyphen are allowed. QindaQt
already implements that canonical rule independently at
`src/shell_visibility/src/compositor_visibility_state.cpp:15-41`,
`src/services/session_lock_state/src/session_lock_state_monitor.cpp:43-53`, and
`src/services/display_service/src/display_inventory_validation_p.h:10-39`.
Because every owner arrival, loss, item event, and request passes this tray
validator, a conforming nonnumeric or multi-element unique owner is completely
unusable. This contradicts ADR-0032's exact-unique-owner decision and must be
repaired with exact 255-byte/ASCII element-boundary tests, including valid
letter/underscore/hyphen and three-element names plus empty-element and
overlength refusals.

## P1 — hostile icon lists are copied before the bound

`validateIconPayload` constructs `icon.pixmaps + icon.attentionPixmaps` and
only then calls `validatePixmapList`, whose count guard is inside that callee
(`src/shell/status_notifier/src/status_notifier_validation.cpp:37-49,169-188`).
`QList::operator+` therefore allocates and copies both attacker-controlled
lists before the advertised aggregate `kMaxIconPixmaps` limit is checked. The
registry's single descriptor gate cannot be called bounded or fail-closed while
validation itself performs work and allocation proportional to an already
over-count payload. Current tests exercise one list just over the count and do
not prove that aggregate refusal precedes combination/copy
(`tests/shell/status_notifier/tst_status_notifier_values.cpp:168-183`).

Repair should check each size and their overflow-safe aggregate before any
combination, then validate each list in place. Add exact aggregate boundary and
split-over-limit cases; no concatenated temporary should remain.

## Next action

Preserve this exact commit. I am continuing copy/lifetime, generation,
request-intent, presentation, CMake/install, documentation, and non-vacuous
test review before the terminal verdict. No product edit, compiler, bus, GUI,
session, input, or host configuration was used.
