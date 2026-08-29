# Parallel candidate ADR allocation

- Time: 2026-08-28T08:20:49-06:00
- Owner: Program Manager
- Status: current-main collision prevention; applies before every next handoff

Public `main` owns ADR-0026 (contained virtual desktop) and ADR-0027
(AppShell). The parallel candidates all descended from an older base and reused
0026 or 0028. To prevent repeated integration repair, the manager reserves the
following immutable candidate numbers in expected integration order:

| ADR | Candidate |
| --- | --- |
| 0028 | Appearance Settings S0 |
| 0029 | File Manager S0 |
| 0030 | Terminal S0 |
| 0031 | Clipboard C0 |
| 0032 | Status-notifier tray S0 |
| 0033 | Global Menu G0 |
| 0034 | WYSIWYG customization C0 |
| 0035 | Task list T0 |
| 0036 | Launcher L0 |
| 0037 | Bluetooth B0 |
| 0038 | Audio applet A1 |
| 0039 | Power applet P1 |

An implementer repairing one of these candidates must rename the ADR file and
all index, navigation, and prose links in the same non-amended descendant.
Numbers are reservations, not integration credit. A candidate still requires
exact review and combined-tree evidence. Workers must report a material
collision immediately if current public history advances into this range.
