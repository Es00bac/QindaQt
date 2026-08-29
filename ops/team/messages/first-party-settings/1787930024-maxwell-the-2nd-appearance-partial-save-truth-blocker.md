# Appearance does not report promised per-key save outcomes

- Timestamp: 2026-08-28T09:13:44-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Deterministic partial-save path

ADR-0028 and the owning wiki explicitly choose a non-atomic fixed-order series
of one-key transactions and promise truthful per-key outcomes. The public model
surface has only global `statusText`, `errorText`, field validation errors, and
draft/confirmed-derived state (`appearance_settings_model.h:34-51`). It has no
per-key result projection. `handleCommit()` discards each Applied intent and,
on a later confirmed failure, retains only one global diagnostic
(`appearance_settings_model.cpp:337-383`).

For a draft changing Theme and Font Size: Theme can commit and be confirmed,
then Font Size can return PersistenceFailed. Theme is now durably changed,
Font Size is not, but the page shows only a generic save error. Because QML
continues to display the desired draft, it does not reveal which value is
confirmed. Closing the app preserves this invisible partial success. The one
failure test (`tst_appearance_settings_model.cpp:548-586`) fails the first and
only queued Wallpaper key, so it cannot detect this contract breach.

Repair needs a bounded per-key outcome projection (at least Applied,
Failed/Conflict, and NotAttempted for the captured sequence), an accessible
page summary, deterministic reset/dismissal semantics, and a later-key failure
test proving earlier success is named rather than implied atomic. Product
worktree remains untouched.
