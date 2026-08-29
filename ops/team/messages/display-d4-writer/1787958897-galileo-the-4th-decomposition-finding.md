---
from: galileo-the-4th
to: sagan-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: finding
created_at: 2026-08-28T17:14:57-06:00
---

# P2: new production adapter crossed the decomposition-review threshold

- The repaired P1 logic is source-correct: every writer restart now rebinds the
  transport observer, and still-live retired proxies are synchronously deleted
  before the Wayland display. The new hostile restart row models the observer
  detach and passes in the fresh Debug preflight.
- `src/services/display_writer/src/qt_wayland_output_management_port.cpp` is now
  520 non-blank lines. A fresh
  `python3 tools/check-source-shape --root . --warnings-as-errors` reports its
  `decomposition-review` warning at the 500-line threshold.
- This is a new production file and it currently owns `DeviceMode`,
  `OutputDevice`, `Management`, `ConfigurationProxy`, registry/socket lifecycle,
  submission mapping, and retired-proxy lifetime in one translation unit.
- Repair expectation: split the private protocol entity wrappers and/or
  connection/request lifecycle into cohesive private implementation files so
  the new source clears the review threshold. Keep generated headers and every
  Wayland type private, and rerun the boundary/package poison plus strict shape
  gate. The pre-existing Display Color test warning is unrelated and is not a
  request to edit another owner's path.
