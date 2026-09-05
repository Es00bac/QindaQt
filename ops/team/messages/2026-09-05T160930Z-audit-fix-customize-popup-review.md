# Independent popup candidate review

Decision: ACCEPT exact candidate 18e546848aaa8084460c7ebc5dab9188c9e8e26b.

Reviewed complete source/test diff for Launcher, Bluetooth, Power, global-menu
facade/QML and child launch environment. Separate popup windows remove parent
surface clipping; Bluetooth Escape shortcut follows popup content and preserves
pairing cancellation. Published string generations fence rendered QML actions,
including ID reuse and unchanged labels across provider lineage; item publication
also dismisses stale menu stacks. Natural menu sizing is independent of assigned
extent. XDG homes and authentication socket/file paths survive launch sanitizing.

Independent test execution in `.cache/fix-popups/build/audit`: 19/19 CTests exit0
for Launcher offscreen/runtime-boundary, Bluetooth offscreen/surface/boundaries,
Power offscreen/boundaries, global-menu access/composition/QML suites/transport
and runtime boundary poisons. Additional launcher-executor 1/1 exit0. Combined
20/20. Read tests for missing assertion paths and reviewed sources at exact hash;
no candidate code changes were made.

Investigated Power confirmation focus: a /tmp-only rebuild of the existing test
with its explicit `requestActivate()` rescue removed leaves focus on a hidden
popup under offscreen. A separate minimal Qt QWindow parent/Popup/hide program,
with no QindaQt code, reproduces that same hidden-popup focus behavior. This is
therefore an offscreen backend limitation, not established product failure.
Manager's private nested gate must check confirmation Cancel/accept-return and
subsequent Escape without artificial focus rescue, plus popup extent at panel
edges. Native compositor focus is outside these offscreen tests' evidence.

No blocking findings. Preserve the exact candidate for manager integration and
run planned nested acceptance. Review probes and logs are temporary at
`/tmp/power-popup-independent-review`; broad focused log is
`/tmp/popups-independent-tests.log`.
