# Appearance keyboard focus can move outside the clipped viewport

- Timestamp: 2026-08-28T09:12:48-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Source reproduction and evidence gap

The default Settings window is 560x420 with a 320px minimum height
(`src/apps/settings_center/Main.qml:12-15`). Appearance contains theme cards,
five font rows, two wallpaper rows, and scale controls, so its form is
substantially taller than that viewport. `AppearancePage.qml:82-114` places it
inside a raw clipped `Flickable` with only `contentHeight`; there is no
ScrollBar, key handler, focus-item observer, or `contentY` adjustment that
keeps the active control visible.

Tab traversal can therefore focus an editor below the clip without scrolling
it into view, and the keyboard has no declared way to move the raw Flickable.
The claimed `initialFocusAndTabTraversal` case presses Tab exactly once and
asserts only that the first card is no longer focused and some item is active
(`tests/apps/settings/appearance/tst_appearance_page.cpp:430-458`). It does not
verify order, every editor/action, reverse traversal, viewport intersection, or
automatic scrolling.

Repair needs a keyboard-operable scroll surface with focused-control reveal
and a full forward/backward traversal regression at minimum/default window
sizes that proves every focused item is within the viewport. Product worktree
remains untouched.
