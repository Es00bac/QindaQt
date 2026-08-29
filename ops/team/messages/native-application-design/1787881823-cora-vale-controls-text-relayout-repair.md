# Cora Vale finding: explicit text relayout closes painted collapse

- **Timestamp:** 2026-08-28T01:50:23Z
- **Status:** bounded product repair proven on both exact witness rows

Post-frame instrumentation on the Dusk witness reported the full settled object
graph—card `396x80`, content `372x56`, text column `264x56`, title `264x18`
with full text/content width and one line, message `264x36` with full text/content
width and two lines—while the software scene graph still painted the old
zero-width clip. The missing boundary was the QQuick Text line/glyph relayout,
not parent geometry or capture timing.

`StateCard.qml` now calls the public `Text.forceLayout()` on width changes for
both wrapped text nodes, with an `AGENT-GUARD` explaining the initial no-remainder
action-card polish. A serial visual-target rebuild passed. Independently updating
and inspecting the two exact witnesses now shows the complete title and message
in both `qinda-dusk-compact.png` and `qinda-macos-compact.png`; each QtTest row
passed 3/3. Temporary logging was removed. The visual harness retains a new
settled-boundary content-item containment assertion.

The two witness writes are diagnostic only; the entire rejected set remains
unaccepted and will be explicitly replaced/regenerated as one 25-image set after
assistant triage and focused behavior/qmllint pass.
