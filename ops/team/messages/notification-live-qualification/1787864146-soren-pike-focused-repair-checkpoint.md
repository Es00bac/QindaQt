# Notification live qualification — focused repair checkpoint

The manager-reported offscreen failure was reproduced exactly. It exposed two
concrete test/presentation facts:

1. `findChild()` cannot reliably discover dynamically incubated `ListView`
   delegates through the center window's QObject ownership tree, so the new
   test now waits on the actual `nextItemInFocusChain()` result instead of
   weakening production behavior.
2. Traversal caused the overflow `Menu` to instantiate and exposed an actual
   delegate defect: the integer `Repeater`'s required `modelData` was undefined
   in that Controls containment path. Both primary and overflow delegates now
   consume the stable integer `index` role and resolve the same action records.

The center retains Qt Quick's natural focus chain; no header-only
`KeyNavigation` edge was restored. The fixture uses a declarative nested action
model and a nonempty history so the enabled forward/reverse chain must contain
primary action, More, Dismiss, Settings, Do Not Disturb, and Clear history.

Exact evidence:

- focused Debug build at `--parallel 1`: exit 0, 146/146 build steps;
- `compositor.development-input-protocol`: pass (6/6 test functions);
- `compositor.kwin-development-input-injector`: pass (5/5);
- `qindaqt.session-supervisor`: pass (12/12);
- initial `qindaqt.notification-surfaces-offscreen`: fail (4 passed, 1
  failed), reproduced once after the first fixture adjustment;
- repaired exact rerun
  `ctest --test-dir build/notification-live-debug -R '^qindaqt\\.notification-surfaces-offscreen$' --output-on-failure`:
  pass, 1/1 CTest and 5/5 QML test functions in 0.18 seconds.

No nested compositor/session, sanitizer, Release, package, or full-registry
process ran. Those remain gated by manager resource coordination.
