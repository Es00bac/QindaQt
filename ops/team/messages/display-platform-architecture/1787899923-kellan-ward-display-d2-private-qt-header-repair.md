# Kellan Ward — Display D2 private Qt header finding and repair

- Time: 2026-08-28T06:52:03Z
- Exact base/worktree: `7da3300c` in `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Current diff: 27 paths, +2,643/−25
- Build/runtime: zero; source/static only

Static inspection of the installed Qt 6.11.1 headers showed that `isValidUniqueConnectionName` exists only in `QtDBus/private/qdbusutil_p.h`; there is no public `QtDBus/QDBusUtil` header. The first source draft included that nonexistent public spelling in the decoder and projector/source, so it would both fail compilation and violate the no-private-platform-API boundary.

I removed that dependency and added one module-private, allocation-bounded grammar for D-Bus unique owners: 4–255 ASCII characters, leading `:`, at least one dot, nonempty elements, and only alphanumeric, underscore, or hyphen characters. Both hostile JSON boundary decoding and the live exact-owner source use the same function. The existing test rejects a replaceable well-known name; the implementation also rejects Unicode, empty elements, missing dots, and overlong values before any source state is accepted.

Direct header evidence confirms public `QDBusConnectionInterface::serviceOwner`, `QDBusReply`, `QDBusServiceWatcher`, and `WatchForOwnerChange` are available. `git diff --check` and source-shape 968 both pass after repair. I remain source/static-only behind Soren's compiler ownership; next action is the already-requested fresh serial compile/test/package lane.
