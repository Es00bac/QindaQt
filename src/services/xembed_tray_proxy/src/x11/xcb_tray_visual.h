// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QtGlobal>

#include <xcb/xcb.h>

namespace QindaQt::XEmbedTray
{

// The visual the proxy advertises through _NET_SYSTEM_TRAY_VISUAL and uses
// for its container windows. A 32-bit TrueColor visual is preferred so ARGB
// clients (Wine draws alpha) keep their transparency; when the screen has
// none, the root visual is used and captures read opaque.
struct TrayVisual
{
    xcb_visualid_t visualId = 0;
    quint8 depth = 0;
    xcb_colormap_t colormap = 0;
    quint32 blackPixel = 0;
};

// Picks the tray visual for the screen and, when it differs from the root
// visual, allocates the matching colormap and its black pixel (both are
// required when creating a window whose visual differs from its parent's).
// Must be paired with freeTrayVisual unless the root visual was kept.
[[nodiscard]] TrayVisual pickTrayVisual(xcb_connection_t *connection,
                                        xcb_screen_t *screen);
void freeTrayVisual(xcb_connection_t *connection, xcb_screen_t *screen,
                    TrayVisual *visual);

} // namespace QindaQt::XEmbedTray
