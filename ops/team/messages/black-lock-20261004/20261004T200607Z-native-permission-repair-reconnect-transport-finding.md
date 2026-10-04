# Reconnect metadata recovery and broad fixture finding

- Worker: `/root/native_permission_repair`
- Time: 2026-10-04T20:06:07+00:00
- Status: working
- Exact base: eeed1f6cac509990f64fd1f692a796f370206c55
- All source/build/tests: qinda isolated `container-wm-keyring-reconnect-20261004`

Manager source review correctly identified that a transient GetNameOwner/UID lookup failure must not be treated as confirmed absence or permanent mismatch. Current lookup is now tri-state: exact unique owner, confirmed NameHasNoOwner, or unavailable metadata. Unavailability retains bounded retries and clears admission; an arrival hint can reset a fresh owner's budget but every actual Attach requires resolved current owner and same UID. Stop still suppresses Shutdown on unknown ownership.

New meaningful private-bus test pauses only the registry whose UID and /proc parent match this test's own dbus-run-session parent, resumes it from a separate bounded thread, and proves same-owner admission recovers after the actual250ms lookup deadline without another name-change signal. Final standalone CTest1/1,11Qt0fail/skips,11.80s; source build0 with qinda -j24-l24. No live bus or production process is paused.

Actual native daemon/software compositor observer gate already passed2/2: replacement restores native screen+idle observers and retains enabled preferences; non-native display remains fail-closed and unadmitted daemon survives stop. Frozen unchanged-eeed owner-replacement negative control fails as expected exit1,2pass/1fail, second attachcount0 versus1.

Broad normal suite before final metadata repair:4/5CTest passed; unchanged general supervisor case failed because its public default `/usr/bin/qindaqt-night-light-service` launched an installed optional role, producing stopcount4 versus expected3. Keyring program is empty there. Public options, production supervisor and original case are byte-unchanged from exactbase. Narrow fixture options now explicitly disable unrelated installed Night Light in14 synthetic cases; dedicated Night Light role tests remain untouched. Manager confirmed this is within focused test ownership. Final production build/normal suite/docs run remains pending; no passing broad claim yet.

Root current-session retained API helper is separately healthy; old running supervisor is not retrofitted by this source. No live credentials/Unlock/Prompt/Attach/config/service/app/desktop operation or laptop build/test/GPU activity by this worker.
