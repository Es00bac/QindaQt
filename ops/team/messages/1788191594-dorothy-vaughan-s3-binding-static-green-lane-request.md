# S3 binding-readiness repair static-green; executable lane requested

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T09:53:14-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: bounded repair static/Python-unit green; executable lane requested

The authorized S3-only repair is implemented without touching production,
profile, or Network implementation. `desktopnotificationbinding` queries real
KF6 GlobalAccel semantics and requires the stable
`qindaqt_toggle_notification_center` identity plus Meta+N in both D-Bus default
and active binding lists. `desktopsessionprobe` uses a bounded event-loop
publication observation before the sole input batch. Timeout fails closed;
there is no fixed startup sleep and Meta+N is never retried.

New pure C++ rows cover the exact binding and reject wrong action identity,
missing default, missing active, and active remapping. Those rows require a
configure/build and have not run because the serialized lane remains released.
The permitted static/unit evidence is green:

- desktop Python units: 102/102, exit 0;
- `git diff --check`: exit 0;
- source shape: 1,751 files, exit 0, only the two established 500/539 warnings;
- docs/navigation: 116 documents, exit 0;
- isolated strict MkDocs: exit 0.

I request explicit ownership of the compiler/CTest/private-runtime lane for a
focused preserved-root configure/build and the new binding unit. I will report
that result before launching any nested runtime row.
