# Android renderer source verification

- Time: 2026-10-09T04:38:14+00:00
- Base: 00aad665c664c5642a565223194926a3698c0cb6
- Branch: worker/android-renderer-evidence-sol-20261009
- Live outcome: concrete AndroidRendererEvidence boundary and credential/lifetime getter, no compositor registry/publisher hook.
- Focused compile: exit0,24.180s,723MiB peak,zero swap;2CPU/2GiB/-j2,120s hard cap.
- Smoke: exit0,2/2 CTest,0.68s,14 Qt methods,0fail/0skip. Existing owned-fork credential/exec/no-replay cases plus new native/PID/provider/death/runtime-replacement cases.
- Units observed settled; small lane released to Program Manager for Portal. No VM, guest, package action or broad rebuild.
- Source caveat: platform evidence only for a trusted explicitly qualified in-process producer/renderer. Separate collector cannot lend its PID to another window; guest AppKey/package/task/surface identity stays unknown. No production origin badge is assigned.
- Next: short strict docs/link gate, freeze exact source, independent review. Production renderer/bootstrap and authenticated guest collector/window publication stay gated.
