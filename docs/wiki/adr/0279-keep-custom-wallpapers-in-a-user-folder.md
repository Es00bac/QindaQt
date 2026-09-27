# ADR-0279: Keep custom wallpapers in a user folder

- Status: Accepted
- Date: 2026-09-27

## Context

Appearance's wallpaper chooser called a C++ QUrl method from JavaScript and
could not reliably select its accepted file. Its gallery listed only bundled
images, so custom choices had no reusable library.

## Decision

An Appearance-owned `UserWallpaperCatalog` owns the local gallery folder,
image validation, non-overwriting copies, and a directory watcher. It accepts
QUrl directly from the QML dialog; C++ converts local URLs to paths. The default
is XDG Pictures/Wallpapers. A local INI preference under the qindaqt config
directory records a user-chosen folder; this UI library preference is not a
Settings1 appearance value. Users can also add/remove files using a file manager.
No database, background service, managed move, or image deletion is introduced.

An imported image becomes a normal absolute-path wallpaper draft. The existing
Settings1 Apply/Revert boundary retains authority; its Apply button reads
**Set wallpaper** on the wallpaper destination. Importing files and choosing a
library folder are immediate local actions, so Revert does not undo them.

## Consequences

Originals and colliding gallery files are preserved. Failed import does not
change the selected wallpaper. Bundled identities and manual paths stay
compatible. The folder is local to each machine; this does not add image sync.
A directory watcher refreshes ordinary changes, with an explicit Refresh button
for filesystems that cannot reliably notify. Existing shell wallpaper resolution
continues unchanged. See [Appearance Settings](../apps/appearance-settings.md).
