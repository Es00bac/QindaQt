# September 7 desktop checkpoint

The Gentoo desktop package `0.1.0_pre20260907-r1` is built and installed.
Release tagging is waiting for the native CI boot checks.

## Saved workspaces

Press **Meta+Ctrl+W** to open Saved workspaces. **Save current** remembers the
active container's name, color, layout, and applications. To bring it back,
select the saved workspace and choose **Reopen**. Choose the window for each
place, then select **Restore workspace**.

Several Terminal windows can be assigned separately. Missing applications are
shown clearly; you can choose another open window as a replacement. Where the
application is installed, **Launch application** opens it, and **Refresh
available windows** updates the choices. A conflicting choice stays visible so
you can correct it; Restore remains disabled until the choices are valid.

Native Save, Reopen, and Rename dialogs now receive keyboard input correctly.
Container roll-up and iconify controls remain available for managing groups.

## Everyday controls

Volume keys show feedback, Print uses Spectacle, and the session starts KDE's
privilege-prompt agent. Power settings control idle display-off timing through
PowerDevil, including application-requested idle inhibition.

## Package and verification

- Gentoo package: `gui-wm/qindaqt-desktop-0.1.0_pre20260907-r1`.
- Immutable application source: `8486e0588e8dd8c21f176be162b53d81ee73a5b6`.
- Required KWin version: **6.6.6**.
- The old package recipe and local binary rollback package are retained.

The Portage build used all 24 available compilation threads. The installed
package passed a two-session virtual-display check: save a named, colored
layout; stop the compositor; reopen with explicitly chosen windows; compare
the restored split tree; save again and verify its identity. Focused dialog
tests also cover correcting duplicate choices.

A private installed-session check exercised real volume changes, shell
feedback, and a decoded Spectacle capture. The real desktop session showed its
privilege prompt. A private installed-session check changed the idle setting
from 10 to 11 minutes and removed the override: the real desktop-controls
process updated all PowerDevil profiles from 600 to 660 and back to 600 seconds
without a diagnostic refresh call. Separate private checks exercised real
PowerDevil idle inhibition and display-off after inhibition ended. This evidence does not claim a hardware
matrix or unrestricted automatic application-session restoration.

See [Gentoo installation](gentoo-desktop.md), the [release procedure](releases.md),
and [KWin upgrades](kwin-upgrades.md) for installation and maintenance.
