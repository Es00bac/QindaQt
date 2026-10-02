# REJECT exact native capture consumer fixture

- Time: 2026-10-02T00:07:19Z
- Exact candidate: `ca29910d15c99319834dd5a88c2485339fc1d015`, accepted base `a0c0cae7fac8ccaab29eb8a52d847ef1c47ced35`, includes c03 recovery fixture prerequisites.
- Verdict: **REJECT P0=0/P1=1**, bounded fixture authenticity/no-default-fallback contract. The separate exact68c fork lifetime ACCEPT remains unaffected.
- Isolated exact qinda tree/branch: `~/.cache/pf-capture-consumer-review-sol-20261002`, `review/pf-capture-consumer-sol-20261002`, preserved/pushed in qinda hub. No product edits or build/private resource claims in this review.
- Reviewed assigned eight paths: `tests/services/portal/capture/CMakeLists.txt`, `capture_pixels.cpp`, `pipewire_frames.cpp`, `pipewire_frames.h`, `run_native_capture.py`, `tst_native_capture.cpp`; `docs/wiki/development/testing-harness.md`, `docs/wiki/reference/portal-capture.md`. Combined assigned delta383insertions/48deletions. Historical ops records outside these assigned product/docs paths remain preserved.

## P1 — decoder permits initial default-target fallback

Position: `tests/services/portal/capture/pipewire_frames.cpp:48` stream properties and `:63–64` connect flags. It correctly resolves the exact offered node ID through its actual returned remote, waits on matching core sync within2seconds and validates a positive serial, then supplies target.object with that authentic serial. But the stream has no `node.dont-fallback=true`. `PW_STREAM_FLAG_DONT_RECONNECT` only prevents later reconnection; it does not prohibit initial default-target linking.

Causal sequence: offered node exists at the registry barrier; it retires before WirePlumber's initial target selection. The defined serial no longer matches. Installed policy lets the failed explicit-target lookup proceed into its default-target hook, which can select an available different node. The decoder counts/checksums/colors whichever node gets linked; logging the previously resolved serial does not prove linked frames originated from it. This violates the explicit no-default-node acceptance contract and both primary docs' claim that retired targets fail closed. This is a source/policy defect; this review does **not** claim an actual native PipeWire race or observed false-positive capture occurred.

Primary evidence directly inspected:

- Qinda installed `/usr/include/pipewire-0.3/pipewire/stream.h:73–76,548–556`: target.object is object.serial/name, target_id should be PW_ID_ANY; implementation's ID-to-serial fix is correct.
- Qinda installed `keys.h:182–186`: dont-reconnect initially uses specified or default target, then destroys on removal.
- Exact upstream PipeWire1.6.8 `stream.c:2091–2093` maps the DONT_RECONNECT flag only to node.dont-reconnect=true; it does not set dont-fallback. Downloaded read-only primary source retained in own ignored `build/review-docs/pipewire-1.6.8-stream.c` SHA256`3699792e732ec7a212045c532289db9b677241500cb5d4653cce20e569b43b55`.
- Qinda installed WP `find-defined-target.lua:38,117–127`: only dont-fallback stops missing-target processing; `find-default-target.lua:29–38` then selects default. The [official PipeWire property reference](https://docs.pipewire.org/page_man_pipewire-props_7.html) distinguishes these two properties.

Executable source-policy reproduction (no compiler/private bus/GPU):

`lua5.4 ~/.cache/pf-capture-consumer-review-sol-20261002/build/review-docs/fallback-hook-proof.lua`

The proof executes the two **unchanged installed policy hooks**, with synthetic event/dependency inputs representing missing requestedserial27 plus an available different defaultserial991. Candidate-equivalent target.object27/dont-reconnect=true selects991; positive control adds dont-fallback=true and errors/destroys without default selection. Actual exit0 means both these asserted observations matched; it does not mean candidate contract passed. This is an executable actual policy-hook causal proof, not full native PipeWire/runtime evidence. Log and source retained. Two earlier harness-only setup failures (method receiver/id mocks) retained separately; fixed before drawing the causal conclusion.

- Proof SHA256 `4530818311ee93078678dd7abce8cecd970ff484d8ea27c3e2cc477b8532addb`.
- Installed defined-target hook SHA256 `758a3d25ec9a8064a080c5caaf895624ff217942b913c5ed1839bb318a398c3f`.
- Installed default-target hook SHA256 `4c333ce1c0e510ebe1865805da5321c0155a7f90682d4404a8fed7322a137d1d`.

Requested repair to same implementer: explicitly forbid initial fallback (and preserve exact target/reconnection constraints), add a mandatory actual-private-PipeWire negative proving missing/retired offered target yields no unrelated frames even with a distinct compatible available/default source, retain positive offered-node serial divergence coverage, and update the docs/repair handoff. Exact repaired candidate requires same-reviewer recheck. Parent and graphics implementer received precise positions/proof paths; reviewer does not implement.

## Other reviewed contracts and direct evidence

Authentic Access uses public `PortalFoundationComposition` on a separate private connection/object and actual existing adaptors; the real frontend is introspected before requesting capture. Protected Screenshot/ScreenCast still use the separate compositor-owned broker and actual consent. No synthetic Access grant or public-boundary bypass was added. Task-only desktop identity/entry matches `org.test.CapturePixels`; frontend starts before its immediate registration. Fatal warnings remain for caller/producer; stderr/lifecycle and producer liveness checks distinguish completed first frame from subsequent abort.

Actual Qt surface paint plus public Wayland frame callback, bounded callback timeout and static callback data lifetime replace exposure-only readiness. Callback completion alone is truthfully documented as insufficient for useful captured content. Existing real PNG/fixture-color/PickColor and multiple changed decoded-frame assertions remain. Registry callbacks resolve only exact offered Node ID, validate correlated sync and serial, clear serial on retirement, and select no first node; the P1 above concerns subsequent policy fallback.

Native lock/privacy assertions retain helper and pending writer retirement, retained file removal, denial response2, offered-node removal, bounded stopped decoded-frame count and postlock denial/no helper. Runner keeps actual dependency survival after Qt success; a Qt3pass/compositor death remains failure. Core0, task-only namespace requirement, dead ambient buses, renderer identity and exact renderD128/ROsysfs/no primary-card/input/sound exception remain. CMake test is noninstallable/opt-in with exact driver-prefix guard and skip77 only when unavailable. This review does not treat an unavailable/skipped gate as PASS.

Retained evidence directly inspected on qinda graphics worktree:

- `build/native-order-matrix`: runner1/53.145s, firstgroupQt6pass1fail0skip, decoder0frames/2nodes/targetnotfound. Negative preserved.
- `build/native-serial-matrix`: actual firstgroupQt7pass0fail0skip in8379ms (five behaviors plus init/cleanup), authentic offered node25→serial27 after churn; actual paint/completedcallback/lifecycle and zero producer stderr. Overall runner1/11.902s because next fresh lock initial Screenshot response2/no file; compositor-loss group not reached. This is implementer firstgroup evidence, **not own fullmatrix**.
- `build/native-scene-producer-stderr`: direct raw fatal Qt registration error says empty app identity; runner1/2.031s and no survivors/cores. Earlier opposite-order identified producer ServiceUnknown history remains preserved.
- Own data-only raw PNG verification exit0: earlier native-scene1100x820 image has902000RGBA(0,0,0,0), SHA256`98ebd1a2f3cc35bebe2446558e058e17722f3641f328cfd1361eb35df6dda3e8`; repaired native-order screenshot has902000RGBA(20,200,70,255), SHA256`1050eac4b450858563bb57a8d4b27bd6687b180abed9c19d682765cc3cea04e3`. Evidence in own `build/review-docs/retained-png-audit.json`; real bytes, not prose inferred.

Own prior exact68c review's unchanged ca299 caller/runner lock row can honestly be reused for **that row only**: first-only unmonitored Qt3pass0fail0skip/2708ms, runner0/3.998s, realAMD1002:731f/ownplugin/node25serial25/decodedframes>3 and lock file/helper/node/frame/privacy/dependency-survival assertions. All14PIDs/root absent/34hashes stable/no monitor/retry; evidence `~/.cache/pf-capture-lifetime-review-sol-20261001/build/review-unmonitored/`. Caller SHA256`5d31d9193ea202f74fec8091edb130278aaa4d15b22ee439d81f6e3f7a8149ea`; runner SHA256`50d35a3fa3abda856ee528bcd1aca2bcac651334ebef4c6ab78054b0fa7f33dc`. That success does not cover the decoder race or resolve intermittent startup admission.

## Gates, limits and stopping point

Own exact-candidate `mkdocs build --strict --site-dir build/review-docs/site` exit0/7.44s; `python3 tools/docs_validation.py` exit0/486Markdown/navigation. Scoped eight-path diff-check exit0. Whole candidate diff-check exit2 only for three historical ops reply EOF blank lines, outside assigned product/docs scope; preserved without edits. All six exact candidate source hashes recorded directly; especially decoder SHA256`d950763c2f2d243bbd5f20532977da2532ead234d31c9b45de6bf2402d2fd102`. No own new compile/native replay was performed or claimed; root owns coherent fork compiler.

Stop exactca299 review at bounded REJECT; board available/resourcesnone. Exact68c and earlier power/package verdicts preserved. Recurring startup admission, full seven-case/three-compositor matrix, compositor-loss coverage, PF19 production stage and installed routing remain held. No user host service, source implementation, assertion, delivery recipe or installed choice was changed. Concrete help offered: same-reviewer recheck of no-fallback repair and its actual private negative, then eventual separate startup receipt candidate review. Manager must route the exact repair to the same implementer and recheck pair before integrating this consumer candidate.
