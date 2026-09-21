// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/xembed_tray_proxy/tray_backend.h>

#include <QtCore/QHash>
#include <QtCore/QString>

#include <xcb/xcb.h>

#include "xcb_tray_visual.h"

class QSocketNotifier;

namespace QindaQt::XEmbedTray
{

class XcbEmbeddedIcon;

// Server scanline layout for one pixmap depth, from the connection setup.
struct XcbPixmapFormat
{
    quint32 bitsPerPixel = 32;
    quint32 scanlinePad = 32;

    [[nodiscard]] quint32 strideForWidth(quint32 width) const
    {
        const quint32 pad = scanlinePad == 0 ? 32 : scanlinePad;
        return ((width * bitsPerPixel + pad - 1) / pad) * (pad / 8);
    }
};

// Production XEmbedTrayBackend over libxcb: owns the connection, the
// _NET_SYSTEM_TRAY_S<n> selection, and the dispatch of every X event that
// concerns an embedded icon. All X11 types stay below this boundary; the
// coordinator above sees only quint32 XIDs. See ADR-0229.
class XcbTrayBackend : public XEmbedTrayBackend
{
    Q_OBJECT
public:
    enum class Atom {
        Selection,
        Opcode,
        MessageData,
        Visual,
        Orientation,
        Manager,
        Xembed,
        XembedInfo,
        NetWmName,
        NetWmWindowOpacity,
        Utf8String,
    };

    explicit XcbTrayBackend(QString displayName, QObject *parent = nullptr);
    ~XcbTrayBackend() override;

    bool start() override;
    void stop() override;
    bool ownsSelection() const override { return m_ownsSelection; }
    void claimSelection() override;
    TrayIconHost *createIconHost(quint32 clientWindow) override;

    // Interface used by XcbEmbeddedIcon; not for coordinator use.
    [[nodiscard]] xcb_connection_t *connection() const { return m_connection; }
    [[nodiscard]] xcb_screen_t *screen() const { return m_screen; }
    [[nodiscard]] const TrayVisual &trayVisual() const { return m_trayVisual; }
    [[nodiscard]] xcb_atom_t atom(Atom which) const;
    [[nodiscard]] XcbPixmapFormat pixmapFormat(quint8 depth) const;
    [[nodiscard]] quint32 imageByteOrder() const { return m_imageByteOrder; }
    [[nodiscard]] QString readWindowTitle(quint32 window) const;
    void unregisterIcon(quint32 clientWindow);
    void addDamageWatch(quint32 clientWindow, quint32 damageId);
    void removeDamageWatch(quint32 clientWindow);

private:
    void drainEvents();
    void handleClientMessage(const xcb_client_message_event_t *event);
    void handleSelectionClear();
    void armSelectionWatch();
    bool internAtoms();
    bool checkExtensions();

    QString m_displayName;
    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    int m_screenNumber = 0;
    quint32 m_imageByteOrder = 0;
    TrayVisual m_trayVisual = {};
    QSocketNotifier *m_notifier = nullptr;
    xcb_window_t m_ownerWindow = XCB_NONE;
    bool m_ownsSelection = false;
    bool m_selectionWatchArmed = false;
    bool m_haveDamage = false;
    bool m_haveXTest = false;
    bool m_haveShape = false;
    bool m_haveXFixes = false;
    quint8 m_damageEventBase = 0;
    quint8 m_xfixesEventBase = 0;
    QHash<quint32, XcbEmbeddedIcon *> m_icons;
    QHash<quint32, quint32> m_damageWatches;
    QHash<quint32, XcbPixmapFormat> m_pixmapFormats;
    xcb_atom_t m_selectionAtom = XCB_ATOM_NONE;
    xcb_atom_t m_opcodeAtom = XCB_ATOM_NONE;
    xcb_atom_t m_messageDataAtom = XCB_ATOM_NONE;
    xcb_atom_t m_visualAtom = XCB_ATOM_NONE;
    xcb_atom_t m_orientationAtom = XCB_ATOM_NONE;
    xcb_atom_t m_managerAtom = XCB_ATOM_NONE;
    xcb_atom_t m_xembedAtom = XCB_ATOM_NONE;
    xcb_atom_t m_xembedInfoAtom = XCB_ATOM_NONE;
    xcb_atom_t m_netWmNameAtom = XCB_ATOM_NONE;
    xcb_atom_t m_netWmWindowOpacityAtom = XCB_ATOM_NONE;
    xcb_atom_t m_utf8StringAtom = XCB_ATOM_NONE;
};

} // namespace QindaQt::XEmbedTray
