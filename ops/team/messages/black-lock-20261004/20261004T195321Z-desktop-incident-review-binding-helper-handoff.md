# Exact temporary keyring lifetime diagnostic handoff

Client `/home/cabewse/.cache/native-keyring-binding-support/client.py` SHA256 **f5316a3a65396fa0356f3977faea8167ddbf116e5815d5dbdd55f2a42cdd53a1**,259 lines/232 nonblank. Byte-identical source and cache-only fixtures/README are on qinda. This authorized temporary diagnostic has no repository commit or installed software; its exact handoff boundary is the immutable file hash. Trust/lifetime source base is eeed1f6cac509990f64fd1f692a796f370206c55 and corresponding relevant code is unchanged from deployedab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b.

Only native calls: `AttachSessionWithDisplay('qindaqt-0')` once and `GetPolicyState`. Bus daemon metadata pins compositor and Session1 unique owner/PID/UID plus equal Keyring1/Secret Service unique owner/PID/UID. Existing local unix bus, private connection, explicit no-auto-start messages, exact reply sender/type,0.75s per RPC, continuous name-owner watches and one normal policy poll each5s. No Session1 methods, Wayland clients, secret/session-open/unlock/policy mutation/shutdown/introspection/collection metadata. Output contains only attachment/degraded booleans, whitelisted policy availability/lock flags and process identity.

Final qinda-only tests:

```sh
env NATIVE_BINDING_PRIVATE_FIXTURE=1 dbus-run-session -- python3 private_fixture.py
env NATIVE_BINDING_PRIVATE_FIXTURE=1 python3 disconnect_fixture.py
```

Private main fixture **exit0,13/13 cases,0 failures/skips,16.485s**. Real private bus/synthetic provider checks: persistent selected caller, exact compositor/Session1/daemon PID selection, unequal native/standard owners denied before Attach, compositor name/unique loss, Session1 loss, daemon replacement with no new Attach, explicit rejection with no policy read, canonical display/typed policy bounds, allowed native method wire/no-auto-start flags, SIGTERM with caller unique-name disappearance, retained malformed-policy/timeout and healthy recovery. Every launched client stderr is empty. Owned synthetic bus disconnect **fixture exit0**, expected client **exit1**, attached=false, zero client stderr. No qinda helper/bus survivors found.

Initial failed fixture evidence is preserved in `private-fixture-initial-failed.log`, `private-probe.log` and `private-probe-2.log`: test filter returned False (HANDLED) and consumed service messages; cleanup originally also retained names. Corrected filter to NOT_YET_HANDLED and direct owned-name requests. Actual client switched deprecated GLib signal function to installed GLibUnix; fixture Python3.14 GI deprecation warnings are harness-only. First corrected run also passed13/13 exit0 in16.498s; final cleanup rerun above uses unchanged client bytes.

**Lifetime/caveats:** actual native `SecretService::ownerLost` terminates/wipes the daemon when this admitted caller unique owner disappears. A deliberate SIGINT/SIGTERM/SIGHUP, definite initial service loss/replacement, Session1/compositor loss or bus disconnect closes the client. Retain it for the full current desktop lifetime; installing new supervisor files cannot transfer the caller within this existing session. An ambiguous Attach timeout deliberately retains the possible admitted connection with attached=false/degraded=true and no automatic replay; this is not confirmed attachment. Policy/metadata transient failure after possible admission retains the connection and retries bounded reads. Synthetic fixtures establish client behavior, not the actual server socket/display binding or live policy; root owns that live gate.

Root's refreshed live pins and complete CLI, supplied for root only:

```sh
python3 /home/cabewse/.cache/native-keyring-binding-support/client.py \
  --compositor-owner :1.155356 --compositor-pid 972162 \
  --session-owner :1.155357 --session-pid 972246 \
  --keyring-owner :1.173993 --keyring-pid 1188694 --display qindaqt-0
```

Root reports it independently inspected/froze this hash, re-pinned identities, and launched transient non-autostart `qindaqt-keyring-binding-incident-20261004.service`: PID1620017, caller:1.183464, attached=true/degraded=false, actual IdleAvailable=true/ScreenLockAvailable=true/ScreenLocked=false, lock protections unchanged. That is manager-reported live evidence, not worker execution. Root alone owns the pending Unlock/Prompt and user response. This worker performed no live bus calls, apps, credential operations, install or session change, and all fixtures/tests ran on qinda.

Requested next action: root retains the reviewed caller for this desktop and completes its live/UI gates. Concrete help offer: independently review and run focused private gates for the native worker's exact permanent supervisor repair commit on qinda when supplied. No unrelated queue or worker runtime/build lease remains.
