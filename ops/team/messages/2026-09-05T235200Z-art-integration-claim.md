# Artwork integration claim

Claimed `.cache/qinda-art-integration` at `8b118917aff156421fdc7a9b13be71d7a7598735`. The bounded contract installs `QindaQt` under the freedesktop icon root, installs bundled PNG wallpapers under `qindaqt/wallpapers`, selects the icon theme through validated shipped theme metadata, and exposes discovered bundled wallpapers through Appearance Settings without overwriting persisted theme or wallpaper choices. The shell currently has no wallpaper surface consumer and ADR-0074 defers live application, so this slice keeps that limitation explicit.
