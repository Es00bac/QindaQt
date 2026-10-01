# Removable media claim

The user's outcome is a graphical insertion notification for removable storage,
with mounting/opening, read-only mounting, remembered choices, formatting with a
separate confirmation, and safe removal. Base: fetched origin/main `6f5c132d`.
Implementation is isolated on `feature/removable-media`; no shared integration
checkout is being edited. Backend ownership is `src/apps/removable_media/media_*`
and `udisks_*`, plus session composition and primary documentation. UI ownership
will be separately assigned with an exact seam/base. Acceptance uses fake UDisks
on a private bus and actual QML controls; host devices are read-only inventory
evidence, never formatting test targets. Source commits will be preserved on qinda.
