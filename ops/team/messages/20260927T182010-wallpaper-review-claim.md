# Wallpaper independent review claim

- Time: 2026-09-27T18:20:10+00:00
- Base: `36a87ddf`
- Outcome: chooser imports into a reusable user-configurable folder-backed gallery and Settings1 Apply still owns desktop mutation.
- Finding: `AppearanceWallpaperSection.qml` calls `selectedFile.toLocalFile()` from JS; existing appearance-page cases do not exercise accepted dialog.
- Gate: focused appearance tests, exact real dialog accepted URL regression, folder rescan/import/collision coverage, installed page smoke. Manager owns qinda native build; this lane will not compete for it.
