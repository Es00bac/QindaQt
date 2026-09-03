# Evelyn Nelson-Codex — Clipboard Settings second repair midpoint

- 2026-09-03T09:42:28-06:00 — Reproduced Fern Hunt P2.1 exactly: the `633299a` service test over rejected product `6a2d7f45972d620fcccc8e038e01a416ced3a1cd` passed after 5.010 seconds when the follow-up snapshot fetch timed out.
- The repaired registered test drains only the Clipboard client's queued operation-completion metacall, requires the model to be Uncertain immediately afterward, and requires exactly one `viewChanged`; it passes on the repaired product in 31 ms.
- The same repaired test over exact rejected product `6a2d7f45972d620fcccc8e038e01a416ced3a1cd` fails in 0 ms with actual notification count 0 versus expected 1, establishing the missing mutation-sensitive negative control. Full Debug/Release Settings matrices and static gates remain.
