# Audio projection repair: exact native handoff
- Author: ed-foreign-astra-20261007
- Timestamp: 2026-10-08T01:47:26Z
- Base: a462c415a3678dc8dc96f15bd996abdceea20189.
- Candidate: e053e1f85310ea921fed6f48d470e04f460fade7.
- Branch: worker/everyday-audio-recovery-20261008; pushed explicitly to /home/cabewse/git/container-wm.git.
- Resources: qinda compiler/all private fixtures RELEASED after the final owning gates. No worker host-bus/physical graph/device/install action.

## Concrete behavior and boundary
A valid aggregate device could erase the entire public graph because mixer channel count disagreed with the node map. The private volume collaborator now preserves dictionary channel indices, pads only with known aggregate truth, and publishes an empty unknown channel vector when mixer has excess positions or a required position/aggregate is unknown. Excess positions are not silently discarded into a falsely complete controllable prefix: mixer-api preserves unspecified indices on write. Existing coordinator admission refuses per-channel writes for an empty vector, while observed aggregate volume/mute/default controls and unrelated rows survive.

No protocol validator, public SDK header, unit, session or UI is relaxed/changed. Fixed validator rejection vocabulary is retained in the bounded malformed-backend diagnostic; rejected names, media and raw backend text are not copied. This restores the existing unknown-channel/public admission contract; no new process/dependency/persistence boundary or ADR reversal is introduced. Documentation changes occupy separate Audio observer/projection sections from Platform's SDK closure paragraph.

Production hand-written source sizes are graph494, coordinator495 and volume102 nonblank lines. The cohesive volume extraction prevents these files growing through the500-line decomposition threshold. Public cross-module dependency directions remain unchanged.

## Observer provenance and live observation limit
Observer2a3613664586d947742dce6e729abe049b38b048 compiled strict17 actions and was independently source-reviewed by Media0de4e36c576d02be0efdf5e5ee2659530a34d1c8. Helper SHA256 remains322206012fa8dd5fabaa01f5174fac6fcc423cdd24e64e6a0ba39f02a635bc3f. Parent target's sibling imported-target failure is preserved in .cache/audio-focused-configure-retry.log; exact5f6b95e86 fixes it by declaring the owning WirePlumberObserver target. The final owning build now actually compiles that manual observer target.

Root, not this worker, reports independently compiling/running exact old observer on the laptop: exit2/valid JSON/stderr0, strict invalid-device,9outputs/6inputs/6streams; only outputindex4 map1/volumes4 and inputindex1 map1/volumes2 fail isolated-row validation. Other rows validate. This attributes the live clear-all state to the concrete mapper mismatch without disclosing names. Fixed laptop observation and installed controls remain manager gates.

The helper does not instantiate the Worker/backend, load graph-changing modules, start upstream services or operate devices. Its fixed synthetic capabilities and empty latency-range input make it a mapper diagnostic, not complete Worker equivalence. Both old and final observer command require explicit --observe and manager approval; no worker execution against host graph occurred.

## Actual executable evidence
1. Immutable original a462 graph translation unit included directly in the frozen5f6b test: strict build53 actions exit0; actual original helper test12passed/8failed/0skipped/0blacklisted, process exit8. Two cases each cover device/stream contraction, partial unknown, sparse unknown rejection and wrong sparse-known index compaction. This is actual production-helper execution, not a mirrored model.
2. Final strict focused build uses --parallel24 -- -l24,18 final actions exit0 (prior full fixed compile38actions). Original configure failures and old logs remain.
3. Final CTest8/8 exit0 and80Qt passed,0failed/0skipped/0blacklisted: protocol15,service12,console-binding13,latency-offsets9,channel-projection21,WirePlumber runtime4,latency runtime3,reset lifecycle3.
4. Actual channel fixture passes production decoder/projection -> unchanged strict validator -> real coordinator with fake backend, retaining unrelated rows/defaults. The additional mismatch test verifies zero backend dispatch for a partial channel request and admission of aggregate SetVolume.
5. Both real private PipeWire runtime and latency fixtures now validate every observed snapshot. Existing null graphs/default/volume/mute/channel/virtual-device/restart and latency/reset behavior remain covered; no physical usability inference.
6. Documentation checker530pages exit0; strict MkDocs exit0; existing Settings Audio public-boundary checker exit0; git diff --check exit0.

The original contraction assertion in5f6b expected a truncated prefix. After root's explicit write-authority safety review, final expected value is empty, and a no-dispatch test was added. Therefore final entire suite is not claimed byte-identical to the old one; immutable old failures are retained and the changed assertion is explicit.

## Reproduction and durable logs
Worktree .cache/audio-focused-source/CMakeLists.txt is the ignored standalone owning build recipe; it adds actual protocol/service/owning test subdirectories, uses the project strict warning helper and source symlinks solely for fixtures referencing CMAKE_SOURCE_DIR. No install target ran. Build directory is .cache/audio-focused-build.

Final test regex:
^qindaqt.audio-(channel-projection|protocol|service|latency-offsets|console-binding|wireplumber-runtime|wireplumber-latency-runtime|wireplumber-reset-lifecycle)$

CTest ran --output-on-failure -j1 with private XDG_RUNTIME_DIR/CONFIG_HOME/STATE_HOME/DATA_HOME, both DBUS_SESSION_BUS_ADDRESS and DBUS_SYSTEM_BUS_ADDRESS set to nonexistent sockets, PIPEWIRE_RUNTIME_DIR private and PIPEWIRE_REMOTE nonexistent. Runtime fixtures replace that with their own temporary null graph socket and clear the remote override. No host graph fallback is present.

Logs: .cache/audio-old-build.log, audio-old-regressions.log, audio-final-build.log, audio-final-ctest.log; .cache/audio-focused-build/Testing/Temporary/LastTest.log; .cache/audio-evidence.json contains SHA256s and parsed totals. Documentation logs use .cache/audio-{docs,mkdocs,boundary}.log.

Requested next action: Media independently reviews exact e053 and actual evidence; root runs same reviewed observer with fixed actual mapper, then integrates and repeats owning gates before the separate Portage package/installed controls route. SDK export repair is separately owned by Platform. ED05 recovery source and its pending native tests remain preserved, not abandoned.
