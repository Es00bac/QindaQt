# Required native frontend token repair and public failure observability

- Predecessor runtime: b3670dda6589f4955f794cbdd516b169198b33ae
- Worker: pf-portal-capture-sol-20261001
- Resources: both released; root conditionally authorizes affected fixture build
  after freeze, followed only by two selected probes after integrated Sleep release

Corrected probes both passed frontend setup. Actual Screenshot returned Response2
with no URI, exit1/0.86s, Qt2pass/1fail/0skip. ScreenCast failed before Start because
CreateSession lacked session_handle_token, exit1/30.93s, Qt2pass/1fail/0skip. Raw
commands/status/logs remain build/b3670dda-private-negative. No successful
capture/stream or direct restricted Screenshot2 error was observed. Group audits
completed, RLIMIT0 and76-byte private socket bound checked, no observed crash or
core files in scanned owned directories; no universal host-mutation claim.

Installed primary frontend ScreenCast XML specifies handle_token and
session_handle_token. Primary1.20.4 xdp-session.c349 explicitly rejects a missing
session token. Owned caller fixture now supplies unique valid standard request
and session tokens; the actual frontend still generates/fences handles and the
unchanged backend receives its normal filtered options. Existing assertions stay.

The actual-input helper uses public application event filtering to subscribe to
existing CapturePort::finished when the stack port receives events. Only bounded
public error text is recorded on failure; no private header, production hook,
permission bypass or successful pixel/node seam is introduced. Failed fixture
cleanup prints the owned audit so missing service, native authorization and
other failures can be distinguished. No authority or production policy changes.

Freeze/push, compile only affected native fixture target with declared helpers,
then repeat only the two fresh private cases. Preserve every predecessor and
release promptly before bounded immutable finding/fork-owner coordination.
