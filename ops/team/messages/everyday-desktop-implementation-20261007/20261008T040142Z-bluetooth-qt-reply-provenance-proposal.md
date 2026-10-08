# Bluetooth Qt reply provenance: actual blocker and proposed correction

- Worker: ed-foreign-astra-20261007
- Time: 2026-10-08T04:01:42Z
- Exact diagnostic source: 7d7e66b81afb2c7b2277ea711dc9888ce7ba8df8; product bytes unchanged 814d.
- Actual focused run: 1/3 pass, exit8. Queued Settings feedback passes. Settings window-close now reaches missing existing PortalPermissions static plugin. Radio exact-positive remains false; actual Reply s/b/v metadata all has empty service(), including bus-driver response. Request/deadline gates true.
- No later native rerun or product authority edit. Original 26/29 and subsequent 1/3 logs preserved separately.

## Primary evidence

Official Qt tag v6.11.1 qdbusmessage.cpp service() explicitly returns empty for ReplyMessage/ErrorMessage; this is not a local-loop hypothesis. qt-source cache contains the exact fetched upstream files. QDBusConnection::call delegates to sendWithReply; the specific pending call supplies its reply. Qt provides no usable response sender through this accessor. Incoming MethodCall service() remains the existing authenticated caller seam.

- https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/dbus/qdbusmessage.cpp
- https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/dbus/qdbusconnection.cpp
- https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/dbus/qdbusintegrator.cpp
- https://dbus.freedesktop.org/doc/dbus-daemon.1.html

Read existing Portage archive /var/cache/distfiles/dbus-1.16.2.tar.xz, SHA256 0ba2a1a4b16afe7bceb2c07e9ce99a8c2c3508e5dec290dbb643384bd6beb7e2. No software downloaded or installed for this evidence. Cache dbus-source/provenance.json records individually extracted source hashes:
- bus/connection.c2087–2124: expected reply lookup matches serial, sending connection, and receiving connection.
- bus/policy.c61–71: allow rules default to requested replies only.
- bus/bus.c applies that expected-reply lookup to send/receive policy.
- dbus/dbus-connection.c pending dispatch matches serial; it does NOT independently authenticate the reply sender.

## Proposed minimal supported transport contract

The constructing session/system message buses are trusted brokers enforcing requested-reply policy. This is a deployment trust prerequisite, not inferred from a reply value or from same UID. An overridden broker policy permitting arbitrary unsolicited replies is outside this contract; ordinary installed policy qualification remains required. This change does not modify any broker policy, main daemon sandbox or installed drop-in.

For synchronous authority queries, issue each call to the captured exact unique daemon/BlueZ owner (bus-driver queries use its fixed reserved name) and consume only that call's response. Preserve pre/post current well-known owner, same-user initiating caller, full current pending intent, selected Adapter1 address, deadline, and exact signature/type/count checks. Remove impossible response service() equality only under this explicit broker/call correlation contract; keep inbound method caller checks.

For the asynchronous helper response, retain the specific pending-call watcher bound to captured exact helper unique owner, per-operation ID, full request and random nonce, current helper owner/UID, initiating callback/lifetime, deadline, exact wire signature, nonce and fixed result whitelist. Owner replacement, cancellation, stale callback, malformed/late result and possible-write uncertainty remain refusal/Uncertain; a later Powered property does not retroactively make a failed operation succeed.

## Required actual negative and positive gates before acceptance

- The existing actual QtRadioAuthority exact-positive row must pass alongside all refusal rows with production guards retained.
- Add a real QtRadioPowerPort complete-success row; existing intent rows never finished a successful helper reply and therefore did not qualify that response branch.
- Inject a forged reply from a third private connection built from the actual held request, with exact reply serial/nonce/payload. It must not complete the pending operation; the legitimate captured helper connection then replies and succeeds. This tests broker connection-pair enforcement rather than nonce secrecy.
- Cover wrong nonce/malformed result, canceled/owner-replaced/expired pending request and duplicate reply without retry or optimistic state.
- A deliberately permissive private broker policy is a diagnostic negative control for the stated transport prerequisite, not a supported production configuration or claim of runtime policy introspection.
- Complete the existing owning Settings fixture module closure (PortalPermissions only, no production edits) and remove temporary reply diagnostics after cause/repair evidence is frozen.

Request manager and same Platform reviewer acceptance of this bounded replacement contract before product edits or native continuation. No RW namespace, host radio or installed-control qualification is claimed.
