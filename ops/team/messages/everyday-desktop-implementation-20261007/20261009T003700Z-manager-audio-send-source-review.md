# Manager review: guarded Audio1 send-gain source

- Time: 2026-10-09T00:37:00+00:00
- Exact prerequisite: 1184f55d51cd929a01321d5f8ccf91a4b09a15e5
- Exact product source: 10258ce7182afde994d92216abea6122379ca0c7
- Exact Sol handoff: 4d87cd89b49b43addd548cbaab381a5a5531e205
- Decision: ACCEPT source-only, default-OFF guarded send path.

Independent source review confirmed all audio_service and focused-test paths in
1184 are byte-identical to the previously accepted e000 prerequisite. The
new reconciler admits exact producer/module/playback tuples, keeps new sends
quiet until actual observation, applies independent strip/send/bus amplitude
with the inverse cubic WirePlumber scale, and waits for module destruction
before retargeting. Worker shutdown drains pending destroys before owned-send
stop and Core teardown. The new path is compile-disabled by default and the
build rejects enablement without exact =media-video/pipewire-1.6.8-r1::qindaqt
and pinned Core/module images. Existing legacy routing is retained unchanged
on ordinary builds. The public SetBusTarget immediate result now reports the
pre-change initiating revision; the owner private client round trip passed.

Owner evidence: focused strict production/test build114.909s cumulative;
five existing gates47 Qt rows and final public Audio1 test3 rows passed;
strict MkDocs/538 links and focused source shape passed. No host audio graph,
PipeWire fixed package, PCM, installed mixer or Audio1 foreign-app signal
was qualified. ADR0361 remains Proposed. The optional experiment harness
stays OFF by default and must not be rerun as a coding prerequisite.

Decomposition review: audio_operation_coordinator.cpp reaches504 nonblank
lines after11 lines of same-responsibility pin-lineage repair. It still owns
one operation admission/result sequence; splitting this invariant from that
sequence would obscure the pre/post snapshot contract. It remains under600,
with future unrelated growth requiring decomposition.
