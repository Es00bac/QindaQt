# Sophie Wilson — Clipboard C1 midpoint

At 2026-09-02T21:42:32-06:00 the strict Debug focused build is green. All 12
`^qindaqt\.clipboard-` rows pass, including the real generated
`ext-data-control-v1` client against an in-process `libwayland-server` fake,
sensitive no-read and size refusal, exact-owner private-D-Bus client/service
round trips and owner loss, hostile wire/boundary controls, and the clean
staged installed consumer/artifact row.

The pre-existing Settings schemas both default `services.clipboardHistory` to
`true`; lane ownership forbids changing an existing key. The host itself starts
denied and accepts only confirmed Settings1 Boolean truth. The schema default
migration is documented as a bounded manager-owned integration caveat.

Release build/tests and final documentation/source-shape/diff gates remain.
