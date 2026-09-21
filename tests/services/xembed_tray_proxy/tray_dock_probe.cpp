// SPDX-License-Identifier: GPL-3.0-or-later
//
// Docks a minimal XEmbed tray icon and reports whether the proxy turned it
// into a StatusNotifierItem. This is the only end-to-end proof available
// without Wine installed: a real Wine tray icon is just an X window that asks
// to dock, which is exactly what this does.
//
// Exit codes: 0 proof complete, 77 skipped (no display or no bus), 1 failed.
//
// AGENT-CONTRACT: the probe never touches the session bus or the operator's X
// display. It reads QINDAQT_TRAY_PROBE_DISPLAY and QINDAQT_TRAY_PROBE_BUS and
// refuses to run without both.
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QTextStream>

#include <xcb/xcb.h>

#include <cstdlib>

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

xcb_atom_t atom(xcb_connection_t *connection, const char *name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, 0, uint16_t(qstrlen(name)), name);
    xcb_generic_error_t *error = nullptr;
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(connection, cookie, &error);
    if (reply == nullptr) {
        free(error);
        return XCB_ATOM_NONE;
    }
    const xcb_atom_t value = reply->atom;
    free(reply);
    return value;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QByteArray displayName = qgetenv("QINDAQT_TRAY_PROBE_DISPLAY");
    const QByteArray busAddress = qgetenv("QINDAQT_TRAY_PROBE_BUS");
    if (displayName.isEmpty() || busAddress.isEmpty()) {
        out() << "skip: QINDAQT_TRAY_PROBE_DISPLAY and _BUS are both required\n";
        return 77;
    }

    int screenNumber = 0;
    xcb_connection_t *connection =
        xcb_connect(displayName.constData(), &screenNumber);
    if (connection == nullptr || xcb_connection_has_error(connection) != 0) {
        out() << "skip: cannot reach X display " << displayName << "\n";
        return 77;
    }
    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t screens = xcb_setup_roots_iterator(setup);
    for (int index = 0; index < screenNumber && screens.rem != 0; ++index) {
        xcb_screen_next(&screens);
    }
    xcb_screen_t *screen = screens.data;
    if (screen == nullptr) {
        out() << "fail: no screen on " << displayName << "\n";
        return 1;
    }

    // 1. The proxy must own the tray selection for this screen.
    const QByteArray selectionName =
        QByteArrayLiteral("_NET_SYSTEM_TRAY_S") + QByteArray::number(screenNumber);
    const xcb_atom_t selection = atom(connection, selectionName.constData());
    if (selection == XCB_ATOM_NONE) {
        out() << "fail: could not intern " << selectionName << "\n";
        return 1;
    }
    xcb_get_selection_owner_reply_t *owner = xcb_get_selection_owner_reply(
        connection, xcb_get_selection_owner(connection, selection), nullptr);
    const xcb_window_t ownerWindow = owner != nullptr ? owner->owner : XCB_NONE;
    free(owner);
    if (ownerWindow == XCB_NONE) {
        out() << "fail: nothing owns " << selectionName << "\n";
        return 1;
    }
    out() << "ok: " << selectionName << " owned by window 0x"
          << QString::number(ownerWindow, 16) << "\n";

    // The spec requires the owner to advertise the visual it composites into.
    const xcb_atom_t visualAtom = atom(connection, "_NET_SYSTEM_TRAY_VISUAL");
    xcb_get_property_reply_t *visual = xcb_get_property_reply(
        connection,
        xcb_get_property(connection, 0, ownerWindow, visualAtom,
                         XCB_ATOM_VISUALID, 0, 1),
        nullptr);
    const bool advertisedVisual =
        visual != nullptr && xcb_get_property_value_length(visual) == 4;
    free(visual);
    if (!advertisedVisual) {
        out() << "fail: the owner does not advertise _NET_SYSTEM_TRAY_VISUAL\n";
        return 1;
    }
    out() << "ok: the owner advertises _NET_SYSTEM_TRAY_VISUAL\n";

    // 2. Dock a real icon window, the way a Wine tray icon does.
    const xcb_window_t icon = xcb_generate_id(connection);
    const uint32_t values[] = {screen->black_pixel,
                               XCB_EVENT_MASK_STRUCTURE_NOTIFY
                                   | XCB_EVENT_MASK_PROPERTY_CHANGE};
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, icon, screen->root, 0, 0,
                      22, 22, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    const QByteArray title = QByteArrayLiteral("QindaQt tray probe");
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, icon, XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING, 8, uint32_t(title.size()),
                        title.constData());

    const xcb_atom_t opcode = atom(connection, "_NET_SYSTEM_TRAY_OPCODE");
    xcb_client_message_event_t message{};
    message.response_type = XCB_CLIENT_MESSAGE;
    message.format = 32;
    message.window = ownerWindow;
    message.type = opcode;
    message.data.data32[0] = XCB_CURRENT_TIME;
    message.data.data32[1] = 0; // SYSTEM_TRAY_REQUEST_DOCK
    message.data.data32[2] = icon;
    xcb_send_event(connection, 0, ownerWindow, XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char *>(&message));
    xcb_flush(connection);
    out() << "ok: sent SYSTEM_TRAY_REQUEST_DOCK for window 0x"
          << QString::number(icon, 16) << "\n";

    // 3. A StatusNotifierItem must appear on the probe's own bus.
    QDBusConnection bus = QDBusConnection::connectToBus(
        QString::fromLocal8Bit(busAddress), QStringLiteral("qindaqt-tray-probe"));
    if (!bus.isConnected()) {
        out() << "skip: cannot reach the probe bus\n";
        return 77;
    }
    QElapsedTimer timer;
    timer.start();
    QString found;
    while (timer.elapsed() < 15000 && found.isEmpty()) {
        const QStringList names = bus.interface()->registeredServiceNames();
        for (const QString &name : names) {
            if (name.startsWith(QStringLiteral("org.kde.StatusNotifierItem"))) {
                found = name;
                break;
            }
        }
        if (found.isEmpty()) {
            QCoreApplication::processEvents();
            xcb_flush(connection);
        }
    }
    if (found.isEmpty()) {
        out() << "fail: no org.kde.StatusNotifierItem appeared within 15s\n";
        return 1;
    }
    out() << "ok: the docked icon published " << found << "\n";
    xcb_destroy_window(connection, icon);
    xcb_flush(connection);
    xcb_disconnect(connection);
    return 0;
}
