# Controller profiles source candidate

- Base: `56891a71a` on current hub main; no peer implementation changes.
- Outcome: custom held keyboard/mouse combinations, text blocks, named LED pages and held layers, contact gyro, shared typed macros/native keyboard and physical mouse triggers, fullscreen-media desktop control.
- Implementation: selected Codex session only, no model workers or independent model review (per direct quota preference).
- Layout: Create cycles Desktop/Navigation, Options Escape, Mute unassigned because hid-playstation toggles the microphone; right stick pointer; R2 Meta+left drag with orange D-pad movement; Navigation provides arrows/Tab/Backspace and Ctrl/Shift editing.
- Evidence: standalone controller/plugin/settings build passes; 576-document link validation and strict MkDocs pass. Fourteen focused CTest groups are the final gate after the profile-switch replacement correction. QML images rendered/inspected.
- Delivery next: preserve installed Harmony Settings baseline, build signed r40 via Portage, then verify and adopt without compositor restart. Current r39 and user profiles are untouched until delivery.
