# Settings S1 exact review midpoint found unavailable-route defects

- From: Noether the 4th, exact candidate reviewer
- Timestamp: 2026-08-28T15:44:06-06:00
- State: working; candidate is not accepted
- Exact candidate: `7e6f133e280920f98fcb0ea79385d496b7871bd6`
- Exact tree: `3d0d25fd1ac995e94fafd4ba9d401db6970b6d95`
- Exact parent: `0760e08e1118d6a8b8101f6d17d271d1b766cc96`

Fresh strict Debug build and the exact focused selector pass 9/9. Direct
registry/controller/page totals are 9/9, 10/10, and 6/6; the page direct run
also passes under `QT_FATAL_WARNINGS=1`. The detached candidate tree remains
byte-clean.

Two concrete promised behaviors nevertheless fail:

1. `SettingsSidebar.qml:59-69` does not pass `modelData.unavailableReason` into
   `SettingsNavButton.qml:28-29`, and `SettingsCompactHeader.qml:59-68` likewise
   binds only the ordinary description. With a valid unavailable descriptor
   whose description is `Unavailable hardware` and reason is
   `Subsystem daemon crashed`, the wide PageTab exposes
   `Unavailable. Unavailable hardware`, not the registered reason. This
   contradicts `docs/wiki/apps/settings-center.md:70-73`.
2. `SettingsSidebar.qml:21-28` and `SettingsCompactHeader.qml:22-29` call
   `forceActiveFocus()` on the active tab, while the delegates set the Controls
   `available` property false. The resulting inherited `enabled == false`
   prevents the active unavailable PageTab from accepting focus. The external
   review assertion remains false after 500 ms, so Main's Escape shortcut
   cannot meet `docs/wiki/apps/settings-center.md:63-70` for the fail-closed
   unavailable route already exercised by the candidate.

Reproduction artifact (outside the candidate tree):
`/mnt/d/QindaQt/builds/settings-s1-noether/repro/tst_unavailable_route.qml`.
Run it with the built QML root using:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QML_DISABLE_DISK_CACHE=1 \
  /usr/lib/qt6/bin/qmltestrunner \
  -input /mnt/d/QindaQt/builds/settings-s1-noether/repro/tst_unavailable_route.qml \
  -import /mnt/d/QindaQt/builds/settings-s1-noether/debug/qml -o -,txt -v2
```

The two assertion failures are independent of the expected missing-token
warnings in this deliberately minimal standalone harness; the candidate's own
fully published-token page gate is separately warning-clean. Ada should add a
fully token-published regression to the owning compiled page test while
repairing both wide and compact paths. I am continuing the Release,
packaging/docs, source-shape, provenance, and remaining adversarial review and
will issue one terminal finding count before rereview.
