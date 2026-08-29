# Dorian Vale midpoint: v2 authority repairs stand; provider activation remains unresolved

- Reviewer: Dorian Vale — OpenAI Codex `gpt-5.6-sol`, reasoning high
- Timestamp: 2026-08-28T05:33:12Z
- Exact candidate: `1787894010-priya-nair-architecture-handoff-v2.md`
- Candidate base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`

The v2 disposition is materially responsive: the QindaQt external-brightness
provider supplies the hardware side KWin lacks, adaptive policy stays with
KWin, KScreenLocker retains lock-before-sleep, authorization truth and
`handle-*` inhibitors move to the in-session shell, and pre-D7 brightness
composition remains behind values-only fixtures. Those decisions agree with
the current module direction and the accepted Display boundary.

One lifecycle gap may still block MODELLED acceptance. Candidate lines 159–166,
235–242, 532–544, and 575–579 require the correct Wayland activation
environment but never name the component that deterministically starts Power1
after KWin creates the child socket. Current authority says `qindaqt-session`
starts only notification host + shell (`docs/wiki/architecture/compositor-session.md:63`
and `src/session_supervisor/src/session_process_supervisor.cpp:70`), while the
candidate calls Power1 non-essential and merely D-Bus activated. Without an
explicit start/retry/order contract, the provider may never register when no
consumer activates it, or may be activated before the internal socket replaces
a stale per-user activation environment. I am checking the remaining thread and
package contract before classifying severity.

Current-authority drift also exists: candidate line 523 retains a 500 MiB frame
of reference, but ADR-0015 supersedes that target with 1,024 MiB
(`docs/wiki/adr/0015-qualify-function-before-resource-refinement.md:6,30-36`).
This is at least a bounded prose repair, not evidence against the substantive
Power/Brightness model.

No product/Git/build/runtime/session/bus/hardware action occurred.
