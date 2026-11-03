# QindaLink desktop-app follow-up handoff (review P1/P2 of 2bef28590)

Branch `link/desktop-app-2` from main a0c116cc9. Candidate = the commit adding this file.

## Fixed
- P1.1 Send confirmation is per normalised recipient set (`m_confirmedSets`), not per session; dialog wording updated.
- P1.2 Partial failure names unsent recipients (display names, else numbers) from `detail.unsent`; the draft is kept and "Retry to these" (still an explicit click, asks first if that set was never confirmed) sends only to them. No `unsent` list = all recipients offered.
- P1.3 `rate-limited`, caller-policy `Denied` (`caller-denied`, told apart from the phone's `denied` by the D-Bus error message) and phone refusal each have their own text, also in the reply window. The send error stays until the reader acts (separate `sendError`, not cleared by reloads).
- P2 Anchored regexes (`QRegularExpression::anchoredPattern`) in phone_setup, phone_apps_view_model, mirror_options; concurrent `SetupPhone` for one serial is refused with `error:"busy"`; refreshed first thread page drops deleted threads; Input page no longer hard-codes a gate: it shows whether the phone lists `input` and says the gate reason (locked/inactive/unverified/busy) appears on the phone, because the app cannot read `input.state`.
- Render aids (fake only): `--demo-partial-failure` with `--open-thread`; fake thread 6 is a group.

## Evidence (offscreen, build/app2)
- qindaqt.link-* 26/26 pass (ctest -R 'qindaqt\.link'), including app rows conversation 17, devices 12, phone-apps 11, options 9, setup 7, caller-dbus, caller-coverage.
- `mkdocs build --strict` clean. Renders viewed: partial-failure with retry notice and confirmation wording, input page.

## Caveats
- The caller-denied/phone-denied split relies on the D-Bus error message text in caller_policy.cpp / messages_adaptor.cpp; change both together.
- `docs|links` ctest rows were not run (no build/dev tree here); mkdocs strict was.
