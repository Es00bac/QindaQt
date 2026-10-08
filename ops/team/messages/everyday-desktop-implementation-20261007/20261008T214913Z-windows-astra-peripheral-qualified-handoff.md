# Peripheral Power source and focused native handoff

- UTC: 2026-10-08T21:49:13.557061+00:00
- Worker: ed-windows-astra-20261008
- Exact product source: 276151aca99e1c627fcc5d69b8af79dca4528d7b.
- Exact tested tree: 5feb7a8920dac409062620dd0274c9cabb1fbea3 (board-only descendant).
- Base: 373fa539460bf2f8fc52f168ffad3d892d422b97.
- Branch: worker/everyday-peripheral-power-20261008.
- Review: Voice Astra source ACCEPT at coordination bc722b20f9da234b4b7fe7f163f1ccbc9271bf12, receipt20261008T214500Z-astra-peripheral-power-source-accept.md. No production source changed after review.

## Outcome

Separate read-only peripheral batteries flow from existing UPower enumeration,
through bounded schema1 values and same-owner targeted nonce receipts, to a
public sibling client and compact scrolling Power popup. System supplies,
old Power1 wire/capabilities and laptop aggregate/critical policy are unchanged.
64-row truncation and unreadable rows are visible. Unknown kinds/levels remain
honest; only reported estimates appear. ADR0365, Power architecture/reference
and primary applet documentation accompany the code.

## Actual verification

- Owning main CMake, Debug, strict production warnings, host input OFF.
- First configure: exit1/10.2898s, fixed capture BrokerExecutable install-directory mismatch. Preserved run-20261008T213919Z; no compile/tests.
- Causal configuration: explicit installed /usr prefix, KDE libexec and share dirs; existing guard unchanged. Configure0/65.4s plus generation7.7s.
- Initial broad shell-object dependency build cancelled ONLY at explicit manager instruction. Outer systemd-run0 on requested stop is **unqualified**, not successful build/test evidence. Preserved run-20261008T214054Z with cancellation.json, raw and exact plan/runner. Warm cache retained.
- Manager-routed focused19 owning test binaries: build exit0/164.563451718s.
- Owning22 CTest registry entries: exit0/13.205176030s,22passed/0failed; underlying19QtTest processes total193passes/0failures/0skips (includes init/cleanup), plus3boundary gates.
- Includes original Power values/wire-golden/aggregate/backlight selection, old service publication/operations/upstream/upower/startup/client/Qt transport; new hostile protocol/decoder/independent-domain and genuine private-bus sender/nonce/replay/cancel/replacement/epoch/contradiction/delete/stop tests; compiled64-row QML scrolling/accessibility/laptop-summary/owner-loss test and existing applet tests.
- Strict MkDocs exit0/9.756s; link/navigation checker exit0/.488s,536documents. Runtime boundary poison controls exit0/.076s; boundary also passed in the22CTest cohort.
- Final native unit actual177.838s, outer held Popen2695479/start50773453/pidfd reaped0/177.847344894s. Inner build2695481 and CTest2698457 both held/reaped0. Actual kernel cpu.max800000/100000, memory.max12884901888, swap0,pids256; nice10, affinity0-5,12-17. UnitCPU515.904s,peak3.9GiB. Heavy lease RELEASED to root immediately after settlement.

## Evidence

Ignored raw bundle: .cache/peripheral-native/peripheral-reviewed-source-and-native-evidence.tar.gz

SHA256 `e9fe3e5fe7935d0125f03a3b2c4c0a620146624f0cea187cbfb932cb857da337`; 38405bytes. Includes all three actual attempts,
exact plans/runners, first failures/cancellation, final caps/identity/waits,
LastTest.log and SHA256s of all19 test binaries. Final raw39494bytes SHA256
`3e35f2465b2daef96a6a9c0c0e4b6e693fe9705bccd1538c00822057db95d611`.

## Bounded outstanding gates and next action

Manager integrate exact source after accepted review, rerun owning tests on
integrated tree, compile the owning shell composition in the full production
artifact, then Portage delivery and actual laptop observation of exposed JLab
speaker/DualSense plus removal/reappearance. No worker installation, service
restart or host-device mutation occurred. No physical battery coverage claim.
The standalone shell composition object was explicitly omitted from the focused
pass after its broad dependency fanout; its compilation remains outstanding.

Original Windows fixture retains its truthful focus failure at d3d3ff1605b74ae31a303560d2e1eeef5bf1683c.
Public ordinary Windows runner/catalog extraction remains the next assigned
product direction after manager routing; no work started in another owner tree.

## Changed paths through tested tree

- `docs/wiki/adr/0365-publish-peripheral-batteries-through-power1-receipts.md`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/reference/power1-v1.md`
- `docs/wiki/shell/power-applet.md`
- `mkdocs.yml`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261008T211310Z-windows-astra-peripheral-power-claim.md`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261008T213001Z-windows-astra-peripheral-source-review.md`
- `ops/team/messages/everyday-desktop-implementation-20261007/20261008T213759Z-windows-astra-peripheral-verification-plan.md`
- `ops/team/workers/ed-windows-astra-20261008.md`
- `src/services/power_client/CMakeLists.txt`
- `src/services/power_client/include/qindaqt/services/power_client/peripheral_client.h`
- `src/services/power_client/include/qindaqt/services/power_client/peripheral_transport.h`
- `src/services/power_client/include/qindaqt/services/power_client/qt_peripheral_transport.h`
- `src/services/power_client/src/peripheral_client.cpp`
- `src/services/power_client/src/qt_peripheral_transport.cpp`
- `src/services/power_protocol/CMakeLists.txt`
- `src/services/power_protocol/include/qindaqt/services/power_protocol/peripheral_types.h`
- `src/services/power_protocol/src/peripheral_codec.cpp`
- `src/services/power_protocol/src/peripheral_validation.cpp`
- `src/services/power_service/CMakeLists.txt`
- `src/services/power_service/data/org.qindaqt.Power1.xml`
- `src/services/power_service/include/qindaqt/services/power_service/adapters/production_battery_collaborator.h`
- `src/services/power_service/include/qindaqt/services/power_service/power_collaborators.h`
- `src/services/power_service/include/qindaqt/services/power_service/power_service_coordinator.h`
- `src/services/power_service/src/adapters/peripheral_decoder.cpp`
- `src/services/power_service/src/adapters/peripheral_decoder_p.h`
- `src/services/power_service/src/adapters/production_battery_collaborator.cpp`
- `src/services/power_service/src/adapters/upower_battery_collaborator.cpp`
- `src/services/power_service/src/peripheral_service.cpp`
- `src/services/power_service/src/peripheral_service_object.cpp`
- `src/services/power_service/src/power_service_coordinator.cpp`
- `src/services/power_service/src/power_service_object.cpp`
- `src/services/power_service/src/power_service_object_p.h`
- `src/shell/power_applet/CMakeLists.txt`
- `src/shell/power_applet/qml/PowerApplet.qml`
- `src/shell/power_applet/src/peripheral_presentation.cpp`
- `src/shell/power_applet/src/power_applet_controller.h`
- `src/shell/runtime/powerappletcomposition.cpp`
- `src/shell/runtime/powerappletcomposition.h`
- `tests/services/power_client/CMakeLists.txt`
- `tests/services/power_client/tst_peripheral_receipt.cpp`
- `tests/services/power_protocol/CMakeLists.txt`
- `tests/services/power_protocol/tst_peripheral_protocol.cpp`
- `tests/services/power_service/CMakeLists.txt`
- `tests/services/power_service/tst_peripheral_service.cpp`
- `tests/shell/power_applet/check_runtime_boundary.cmake`
- `tests/shell/power_applet/tst_power_applet_qml.cpp`
