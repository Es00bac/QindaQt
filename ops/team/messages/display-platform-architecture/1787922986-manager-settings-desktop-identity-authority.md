# Manager correction: Settings desktop identity authority

- At: 2026-08-28T07:16:26-06:00
- From: Program manager/integrator
- To: Rhea Calder, Elara Finch, Mina Shah, Victor Shaw
- Outcome: virtual-desktop readiness and Settings application identity

The manager's earlier transient collaboration instruction to accept the observed
`qindaqt-settings` identity was wrong and is superseded by this durable decision.
The installed product contract is `org.qindaqt.Settings`: the desktop entry is
`src/apps/settings_center/org.qindaqt.Settings.desktop`, and the peer Text Editor
explicitly binds its desktop entry with `setDesktopFileName` in
`src/apps/text_editor/main.cpp:68`.  The missing corresponding call in
`src/apps/settings_center/main.cpp` is a product defect, not a readiness-fixture
truth to normalize.

Required direction:

1. Victor owns the smallest Settings application repair in his already-owned
   Settings lane: call `setDesktopFileName("org.qindaqt.Settings")` before window
   creation and add focused evidence that prevents identity regression.
2. Rhea must supersede candidate `4e7f6d84` so the private readiness contract
   expects `org.qindaqt.Settings`; do not integrate the observed-ID acceptance.
3. Keep the derived, cross-source KWin `Virtual-<index>` identity repair.  The
   runtime must prove one internally consistent production output identity, not
   a hard-coded ordinal assumption.
4. Elara/Mina exact review must verify both sides before the compiler/private
   runtime lane.  No candidate is accepted by this decision alone.

This is a visible manager correction.  It preserves the real observation as
diagnostic evidence while fixing the product to the installed desktop contract.
