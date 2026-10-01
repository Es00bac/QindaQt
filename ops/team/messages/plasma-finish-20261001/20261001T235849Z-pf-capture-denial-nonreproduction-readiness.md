# Bounded unmonitored refusal probe and readiness boundary

2026-10-01T23:58:49+00:00

## Exact inputs and outcome

Consumer executable source ca29910d15c99319834dd5a88c2485339fc1d015 (current branch documentation/records only); caller SHA256 5d31d9193ea202f74fec8091edb130278aaa4d15b22ee439d81f6e3f7a8149ea. Fork review68c4d74f903b7e8990dd5fd5d509ec8154eac1d1, executable42604a8f420bf55efe1d582bb2a8b394b916d8d8 plugin aa00b6db3c4ee8bea6ca27b7b44aad9b9c402aae8a3b2afeee52f5a0ccf6d742, original447 driver 5f06d13d4749cc1ace859080ef8ce1b6b28f0a4f65750b82331470dbbc2a1086.

One authorized failure-only diagnostic broker derived from preserved pf-portal-protected-capture build artifacts at source4fe2b6894fa5a58fd4a64cc550c85099c08027fe; all three original source files byte-equal current ca299 consumer files. Existing library objects and compiler flags retained, three explicit diagnostic objects linked before their static archives. No owner artifact modified. Broker SHA25620074346115c04e3eac9d200c4a617c4b737b7885b702037781ce96b9db349d9 is RO-bound over compiled broker launch path only within private namespace; original broker remains SHA256716ab8be38436b4d92641381b5870a6af0263d1d3cdb8e92e80a37aa106a007b. The diagnostic sources only preserve evaluated failure stages and print at actual refusal/failure; no extra admission calls, sleeps, retries, monitor, or assertion changes.

Compile/link exits0/0/0/0,3.006/3.406/2.605/0.402s, minimum16,371,688kB available; compilerPGIDs443569/443586/443604/443638 ended. Exact argv/source/input hashes/build logs: own `build/denial-diagnostic/{commands.json,source-input-hashes.json,build-status.json,command-*.log,binary-sha256.txt}`. The complete source delta is retained below as well as diagnostic.patch.

Command `python3 build/run_lock_denial_diagnostic.py` exits0/13.663s, caller nativeLockStopsActualStreamPendingCaptureAndRetainedFile Qt3/3; original447 driver also3/3. Actual PipeWire node25→serial25. Existing pixel/frame/lock/file retirement and compositor survival assertions all pass. RuntimePGID444292 ends; no survivors, scoped cores, or cleanup errors. Preflight verifies AMD1002:731f renderD128 only, RO sysfs, private namespaces/dead host buses, core0, no primarycard/input/snd. Runtime manifest records the namespace binary overlay explicitly. Evidence root `build/native-lock-denial-diagnostic/`, group `evidence/qindaqt-native-capture-k33w6cu3/`; raw compositor/PipeWire/WirePlumber/producer logs, audit and all prior failed runs retained. No CAPTURE_DENIAL line appears: initial refusal was NOT reproduced. No startup repair or full matrix claim follows from this pass.

## Source-only readiness contract

1. Fork test driver's marker waits only `CaptureAuthority::testBrokerReady()` (live broker control channel plus authenticated broker sender). Consumer Channel replies Ready on Hello, independently of native-lock monitor progress.
2. Broker main registers service before constructing AuthorityCapture, then exports Screenshot/ScreenCast immediately after channel availability. `available()` means channel setup, not `admitted()`.
3. Native admission requires inherited compositor peer credentials and retained pidfd, current canonical unique owner/PID, then asynchronous owner/PID resolution, subscription, and `RequestStateWithReceipt` with fresh nonce. QtNativeLockTransport publishes stateResolved only after both the exact owner's targeted nonce receipt AND empty method reply; monitor generation/serial/identity and unlocked state must still hold. Request-time checks independently repeat authority/privacy validation.
4. NativeCaptureAdmission emits an internal Qt ready signal only after contentMayBeShownChanged(true) and admitted(). CaptureDialog consumes it; AuthorityCapture/backend does not forward it. No public broker property/method or wire acknowledgment states that THIS broker has consumed the required state and is currently admitted.
5. Backend name presence, frontend introspection, producer paint/frame, the harness's own NativeLock receipt, or a bus monitor seeing delivery to the broker do not prove that broker callbacks consumed both receipt and reply and published admitted state. Adding any of those as a wait would be unproven timing manipulation.

This is a source-proven absence of an ordering guarantee, NOT evidence that native admission caused the observed response2. Parse, registry, caller, native/channel, and job-start failures remain possible until a failing instrumented row establishes its actual guard. A future readiness contract, if justified, must acknowledge this broker's actual combined channel/native admission after authentic receipt consumption, correlate the requester/reply and broker owner/generation, fail or retire on loss, and retain every request-time recheck. It must convey ephemeral readiness, never grant capture permission or make a stale readiness observation authoritative. A test-only forwarding of the existing internal transition could expose that authentic boundary; changing production service export/request timing would be an explicit behavior contract change requiring separate review. Neither is implemented.

## Stopping point and requested next action

Both leases released directly to manager/reviewer; independent68c review owns runtime now. No additional run or production policy change. Manager can route one bounded failure reproduction if required, or review the precise readiness contract before authorizing an additive acknowledgment. Offer: implement exact failure-stage refinement or review that boundary after routing. Current failure-only pointer is the last failed admission stage; only a Screenshot ui.admitted refusal makes it current causal evidence. Generic authority.fail output would need its own branch refinement and must not be interpreted as proving the pointer caused that job's failure.

## Preserved diagnostic source delta

```diff
--- src/services/portal/src/screenshot_adaptor.cpp
+++ diagnostic/src/services/portal/src/screenshot_adaptor.cpp
@@ -1,3 +1,5 @@
+#include <cstdio>
+extern const char *qindaqtDenialStage;
 // SPDX-License-Identifier: LGPL-3.0-or-later
 #include <qindaqt/services/portal/screenshot_adaptor.h>
 namespace QindaQt::Services::Portal {
@@ -17,11 +19,12 @@
     // AGENT-CONTRACT: request lifetime bounds60s native selection plus30s
     // protected write/margin. The operation itself never extends its30s clock.
     const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse response) { m_pending.remove(*slot); if (response != RequestResponse::Success) m_ui.cancel(*slot); }, 100000);
-    *slot = token; if (!token) return 2;
+    *slot = token; if (!token) { std::fprintf(stderr, "CAPTURE_DENIAL screenshot.registry.begin\n"); return 2; }
     auto request = screenshotRequest(app, parent, options, color);
-    if (!request || !m_ui.admitted()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
+    if (!request) { std::fprintf(stderr, "CAPTURE_DENIAL screenshot.request.parse\n"); m_requests.finish(token, RequestResponse::Failed); return 2; }
+    if (!m_ui.admitted()) { std::fprintf(stderr, "CAPTURE_DENIAL screenshot.ui.admitted stage=%s\n", qindaqtDenialStage); m_requests.finish(token, RequestResponse::Failed); return 2; }
     request->caller = captureCaller(handle.path());
-    if (request->caller.isEmpty()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
+    if (request->caller.isEmpty()) { std::fprintf(stderr, "CAPTURE_DENIAL screenshot.caller.empty\n"); m_requests.finish(token, RequestResponse::Failed); return 2; }
     m_pending.insert(token, request->kind); m_ui.request(token, *request); return 2;
 }
 quint32 ScreenshotAdaptor::Screenshot(const QDBusObjectPath &h, const QString &a, const QString &p, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(false, h, a, p, o, c, r); }
--- src/services/portal/capture/backend/authority_capture.cpp
+++ diagnostic/src/services/portal/capture/backend/authority_capture.cpp
@@ -1,3 +1,5 @@
+#include <cstdio>
+const char *qindaqtDenialStage = "unset";
 // SPDX-License-Identifier: LGPL-3.0-or-later
 #include "authority_capture.h"
 #include "../authority/channel.h"
@@ -72,6 +74,7 @@
         return caller.isValid() && caller.value() == job.request.caller;
     }
     void fail(RequestToken token) {
+        std::fprintf(stderr, "CAPTURE_DENIAL authority.fail token=%llu last_admission_failure=%s\n", static_cast<unsigned long long>(token), qindaqtDenialStage);
         const auto job = jobs.value(token); if (job && job->published) return;
         QTimer::singleShot(0, &q, [this, token] { if (requests.live(token)) Q_EMIT q.completed(token, RequestResponse::Failed, {}); });
     }
@@ -99,7 +102,7 @@
             // replying to each one would create an endless Revoke/Revoked loop.
             return;
         }
-        if (packet.packet.message == Message::Error || packet.packet.message == Message::JobRevoked) { fail(token); retire(token); return; }
+        if (packet.packet.message == Message::Error || packet.packet.message == Message::JobRevoked) { std::fprintf(stderr, "CAPTURE_DENIAL authority.packet message=%d job=%llu\n", static_cast<int>(packet.packet.message), static_cast<unsigned long long>(packet.packet.job)); fail(token); retire(token); return; }
         if (packet.packet.message != Message::JobStarted || job->started || packet.fds.size() != 2 || !live(*job) || !requests.live(token)
             || !pipeEnd(packet.fds[0], O_WRONLY) || !pipeEnd(packet.fds[1], O_RDONLY)) { fail(token); retire(token); return; }
         const auto fds = packet.takeFds(); job->writeFd = fds[0]; job->readFd = fds[1]; job->started = true;
@@ -157,7 +160,7 @@
     : CaptureUI(parent), d(std::make_unique<Private>(*this, requests, std::move(bus), std::move(runtime), fd)) { d->start(); }
 AuthorityCapture::~AuthorityCapture() { revoke(); d->channel.stop(); }
 bool AuthorityCapture::available() const { return d->channel.available(); }
-bool AuthorityCapture::admitted() const { return d->channel.live() && d->admission.admitted(); }
+bool AuthorityCapture::admitted() const { if (!d->channel.live()) { qindaqtDenialStage = "channel.live=false"; return false; } return d->admission.admitted(); }
 void AuthorityCapture::request(RequestToken token, const CaptureRequest &request) {
     if (!token || !admitted() || !d->requests.live(token) || d->jobs.size() >= CaptureAuthority::Wire::MaxJobs || d->jobs.contains(token) || d->next == std::numeric_limits<quint64>::max()) { d->fail(token); return; }
     if (!d->bus.interface()) { d->fail(token); return; }
--- src/services/portal/capture/native_capture_admission.cpp
+++ diagnostic/src/services/portal/capture/native_capture_admission.cpp
@@ -1,3 +1,5 @@
+#include <cstdio>
+extern const char *qindaqtDenialStage;
 // SPDX-License-Identifier: GPL-3.0-or-later
 #include "native_capture_admission.h"
 #include <qindaqt/compositor_names/compositor_names.h>
@@ -47,6 +49,6 @@
     const auto actual = daemon(m_bus, "GetConnectionUnixProcessID", m_owner);
     return actual.type() == QDBusMessage::ReplyMessage && actual.signature() == "u" && actual.arguments().value(0).toULongLong() == m_pid;
 }
-bool NativeCaptureAdmission::admitted() const { return identityLive(m_owner, m_pid) && m_monitor.contentMayBeShown(); }
+bool NativeCaptureAdmission::admitted() const { if (!identityLive(m_owner, m_pid)) { qindaqtDenialStage = "native.identityLive=false"; return false; } if (!m_monitor.contentMayBeShown()) { qindaqtDenialStage = "native.contentMayBeShown=false"; return false; } return true; }
 bool NativeCaptureAdmission::lineageLive() const { return identityLive(m_owner, m_pid); }
 }
```
