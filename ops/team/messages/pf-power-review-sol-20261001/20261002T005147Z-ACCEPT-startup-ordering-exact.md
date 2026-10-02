# ACCEPT bounded exact capture startup ordering amendment

- Time: 2026-10-02T00:51:47Z
- Exact candidate: `24fa776c9bd038c13d8b0bdd974298fa913253dd`, executable code63e33/production7b79. **ACCEPT P0=0/P1=0 for the assigned startup amendment**. Inherited ca299 decoder P1 is separate and remains REJECT/pending repair; this is not blanket acceptance of that inherited fixture.
- Isolated qinda `~/.cache/pf-capture-startup-review-sol-20261002`, branch `review/pf-capture-startup-sol-20261002`, exact commit pushed hub. No reviewer product edits.
- All ten assigned paths reviewed: `src/services/portal/capture/native_capture_admission.{h,cpp}`, `backend/authority_capture.{h,cpp}`, `backend/main.cpp`; `tests/services/portal/capture/{tst_broker_startup.cpp,CMakeLists.txt}`; ADR0324, portal-capture reference, testing-harness. Historical worker records inspected without modifying others' paths.

## Source/lifetime/security audit

Initialization is current-owner Known native state, independent of Unlocked content admission. The unchanged public monitor/transport validates retained-peer PID/pidfd/current canonical unique owner, subscribes to the exact endpoint, and accepts a matching targeted nonce receipt only together with an empty successful method reply before state resolution. NativeCaptureAdmission.initialized rechecks identity and Known state. Content still requires current-owner Unlocked admission.

Backend assembly delays object registration and public fixed-name registration until initialized. It rechecks before name publication and start rechecks initialized again; order is object→name→Channel.start, preserving the compositor's exact-name authentication of QCC1 Ready. Start is once-only and failure terminal. The pre-start lifetime timer checks native lineage and explicitly avoids Channel.reconcile on its unset identity. Owner replacement before completion prevents publication even if the old endpoint subsequently sends the authentic late reply; inherited generation/serial and read-through checks reject stale state. Once started, Channel identity/credentials/Hello/FD/consent barriers and request-time native admission/job revocation remain authoritative.

The absolute five-second startup timer starts at broker construction, covering native initialization and the delayed Hello phase, and stops only when authenticated Hello is delivered by Channel. The existing channel five-second handshake timer remains; no arbitrary delay or timeout extension is introduced. Initial Locking or Locked may publish availability/Ready while admission denies StartJob. Code stays within the owning private capture boundary; no public wire bytes/descriptors/compositor/helper permission or test authorization changes occur.

New fixture uses an actual private D-Bus daemon, actual protected backend child, actual SO_PASSCRED credentials and current unique-owner/PID queries, delayed targeted native endpoint and actual Peer.Ping round trips. Each withheld-message order checks no public name/no received Ready, then both real messages allow authentic Ready and unlocked StartJob. Locked/Locking return denial without StartJob. Owner replacement plus late old reply checks child NotRunning, no name and no prior/received Ready. ECONNRESET is accepted only after that actual child has stopped; other recv errors remain failures. The original eager backend proves the publication negative. No fake Ready/helper/pixels or host services are involved.

## Own build/docs evidence

`python3 ~/.cache/pf-capture-startup-review-sol-20261002/build/build_exact.py` executed all16 MOC/TU/link commands exit0, summed32.280s, minimumMemAvailable16,340,672kB, core0, bounded120s/command and ownedPGID3GiB30s stop. Four MOC generations, current production/source objects, unchanged channel test and four links produced own separate artifacts; immutable existing dependency archives were hashed before/after and never replaced. All16 compilerPGIDs absent; compiler released before private replay.

Exact argv, source/input/generated-MOC/output manifests, depfiles, logs and build-integrity.json are retained under own `build/exact-startup/`. The ten assigned source/docs hashes are exact and stable. Normalized dependency paths prove current NativeCaptureAdmission/AuthorityCapture headers enter changed compilation, with no old changed-header identity. An initial literal-path proof check rejected a valid `/../../` MOC include spelling; normalization fixed the audit without rebuilding or changing any artifact. Four actual generated sources are hashed. New fixture source SHA256`aafcc783ccfe784b3fefbb6d7903491016fb4e13ae17392ccf97c05f25e738b1`.

Own strict MkDocs exit0/7.25s to ignored build/review-docs/site; repository docs validation exit0/486Markdown/navigation; scoped product/docs diff-check exit0. Docs retain historical native response2 as unproven and the full native/fork/decoder boundaries.

## Own private replay and retained failures

Commands: `python3 .../build/run_private_gates_short.py`; exact args/status/logs in `build/exact-startup/private-gate-short/`.

| Actual gate | Exit / counts / elapsed |
| --- | --- |
| startup-positive | exit0; Qt7pass/0fail/0skip,1024ms; runner1.042s |
| unchanged channel-positive | exit0; Qt13pass/0fail/0skip,230ms; runner.247s |
| original eager-negative, receipt-without-reply | expectedexit1; Qt2pass/1fail/0skip; runner.125s; first failure is actual public name present (`!present.value()` FALSE) |

The unchanged channel13 covers teardown storage, consent/duplicate grants, preconsent/stale generation/wrong job, kernel sender mismatch, owner replacement, FD ownership/job IDs, HUP and ancillary mismatch. Startup7 includes init/cleanup, both withheld-message orders, Locking/Locked and owner replacement. No skipped/unavailable result is counted PASS. Original eager negative backend hash stays `716ab8be38436b4d92641381b5870a6af0263d1d3cdb8e92e80a37aa106a007b`; later helper assertions after the first negative failure are not separate causal evidence.

Own first run is retained under `build/exact-startup/private-gate/`: startup fixture setup exit2/Qt0pass2fail/15.137s at private daemon print-address readiness, before any backend/admission row. Reviewer wrapper TMPDIR was the long evidence path, exceeding Unix socket path capacity after QTemporaryDir appended its directory/bus name. No fixture descendants remained. Corrected only wrapper environment to short owned0700 `/tmp/pf-startup-sol-*`, retaining exact binaries/source/assertions and first failure logs. No source repair/guard weakening or timing retry was used. The approved three gates then passed above.

Own private environment: temporary HOME/XDG, dead ambient session/system buses, no DISPLAY/Wayland, fatalwarnings1, core0; actual tests create their own private daemon/backend. No compositor/GPU/native full matrix. PID start-tick tracking and direct postexit inspection verify all12 final runner/child PIDs plus initial485842 absent; short root and first setup temp children absent; no scoped cores or cleanup errors. All27 frozen source/input/output/eager identities remain unchanged. `verification.json` directly records final cleanup. Compiler/private both explicitly RELEASED; resources now none.

Own output SHA256:

- backend: `b2d4bd941a24a0ac535d072111e4f00ff22667bec2fe1879731f5c048093c7a9`.
- startup-tests: `010037064827abfe7c65192e08f3b11976e2171d37164bbbfcad92fc531e5330`.
- channel-tests: `198c33f89096078d342569b57a9941d7b216a8932d574fe957d4506292358f30`.
- startup-negative-tests: `a690703b64e2e79841615dc99d46fe6cd78ab6838c6d175bd0309a338665975c`.

Retained implementer first startup Qt6pass1fail at terminal seqpacket read, repaired Qt7pass/channel13pass and eager-negative first publication failure were directly read. The narrow ECONNRESET correction preserves no-name/no-Ready/actual-exit checks. All initial native Screenshot response2 evidence remains; its actual refusal cause was not reproduced or proven by this review.

## Handoff/next gate

Manager may integrate the accepted startup amendment and rerun its affected focused gates on the integrated tree. Exact68c lifetime ACCEPT and productionOFF QtTest build incompatibility remain separately preserved. No historical initial-response2 cause, broader native seven-case/three-compositor matrix, decoder acceptance, production routing/stage/install or fullPF19 claim follows from this focused20Qt qualification.

Stopping point: exact bounded verdict posted, board AVAILABLE, resourcesnone, all work preserved/pushed to qinda. Concrete next compatible help: same independent reviewer will inspect the corrected decoder exact candidate/source first, then request only bounded compiler/private graph scopes required for no-fallback positive/negative verification. No wider work claimed.
