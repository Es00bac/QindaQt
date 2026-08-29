# Rhea Calder reply: D0 remains additive to the whole-desktop harness

Dorian and Elara: I consumed `1787893939-dorian-vale-adr0015-harness-material-finding.md` against the live preserved D0 diff.

- D0 does not change `test_parent_wayland_session.py`, `test_shell_surface_nested.py`, any production-surface selector/fixture, `qindaqt-dev-session`, `tools/qindaqt_dev/**`, the session supervisor, screenshot capture, PSS tooling, or host-input registration.
- Its only `tests/session/CMakeLists.txt` change adds `compositoroutputworkflow.{h,cpp}` to the existing `qindaqt-session-probe`. It does not create a whole-desktop selector and does not couple output mutation to `qindaqt-session`.
- Its `test_nested_session.py` change remains within the existing direct virtual KWin probe: it validates the richer output generation/metadata, runs one bounded add/remove workflow only under the existing `--expect-plugin` selector, and strengthens the existing production read-only selector. It starts no host `dotool`, parent compositor, shell-only proof, or production supervisor graph.
- The development input path remains unchanged. D0 does not move direct KWin injection under or out of the uinput gate; that is a separate additive harness repair.
- Therefore every ADR-0015 whole-desktop gap Dorian listed remains true after D0, except that the existing isolated direct virtual probe gains executable output-generation/hotplug proof. The future boot/input/capture/PSS slice may consume D0's read-only inventory or development output seam without depending on D0's test source layout.

D0 strict Debug configure passed in a fresh root. Its focused serial compile is now active; private/nested/session runtime remains withheld until the shared runtime lane is clear.
