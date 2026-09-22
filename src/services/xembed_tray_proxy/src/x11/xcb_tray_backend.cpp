// SPDX-License-Identifier: GPL-3.0-or-later

#include "xcb_tray_backend.h"

#include "xcb_embedded_icon.h"

#include <QtCore/QByteArray>
#include <QtCore/QLoggingCategory>
#include <QtCore/QSocketNotifier>

#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/shape.h>
#include <xcb/xfixes.h>
#include <xcb/xtest.h>

Q_DECLARE_LOGGING_CATEGORY(lcXEmbedTray)

namespace QindaQt::XEmbedTray
{

namespace {

constexpr quint32 kSystemTrayRequestDock = 0;
constexpr quint32 kSystemTrayBeginMessage = 1;
constexpr quint32 kSystemTrayCancelMessage = 2;
constexpr quint32 kHorizontalOrientation = 0;

[[nodiscard]] xcb_atom_t internAtom(xcb_connection_t *connection,
                                    const char *name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, false, quint16(std::strlen(name)), name);
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
        return XCB_ATOM_NONE;
    }
    const xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

} // namespace

XcbTrayBackend::XcbTrayBackend(QString displayName, QObject *parent)
    : XEmbedTrayBackend(parent)
    , m_displayName(std::move(displayName))
{
}

XcbTrayBackend::~XcbTrayBackend()
{
    stop();
}

bool XcbTrayBackend::start()
{
    if (m_connection != nullptr) {
        return true;
    }
    m_connection = xcb_connect(
        m_displayName.isEmpty() ? nullptr : m_displayName.toUtf8().constData(),
        &m_screenNumber);
    if (m_connection == nullptr
        || xcb_connection_has_error(m_connection) != 0) {
        qCWarning(lcXEmbedTray, "cannot connect to X display '%s'",
                  qPrintable(m_displayName.isEmpty()
                                 ? QString::fromLatin1(qgetenv("DISPLAY"))
                                 : m_displayName));
        stop();
        return false;
    }

    xcb_screen_iterator_t screenIterator =
        xcb_setup_roots_iterator(xcb_get_setup(m_connection));
    for (int i = 0; i < m_screenNumber && screenIterator.rem; ++i) {
        xcb_screen_next(&screenIterator);
    }
    m_screen = screenIterator.data;
    m_imageByteOrder = xcb_get_setup(m_connection)->image_byte_order;

    const xcb_setup_t *setup = xcb_get_setup(m_connection);
    xcb_format_iterator_t formatIterator = xcb_setup_pixmap_formats_iterator(setup);
    while (formatIterator.rem) {
        const xcb_format_t *format = formatIterator.data;
        XcbPixmapFormat value;
        value.bitsPerPixel = format->bits_per_pixel;
        value.scanlinePad = format->scanline_pad;
        m_pixmapFormats.insert(format->depth, value);
        xcb_format_next(&formatIterator);
    }

    if (!checkExtensions() || !internAtoms()) {
        stop();
        return false;
    }

    m_trayVisual = pickTrayVisual(m_connection, m_screen);

    // The selection owner window exists for protocol identity only and is
    // never mapped.
    m_ownerWindow = xcb_generate_id(m_connection);
    xcb_create_window(m_connection, XCB_COPY_FROM_PARENT, m_ownerWindow,
                      m_screen->root, -1, -1, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                      nullptr);

    m_notifier = new QSocketNotifier(xcb_get_file_descriptor(m_connection),
                                     QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this,
            &XcbTrayBackend::drainEvents);
    m_notifier->setEnabled(true);

    claimSelection();
    return true;
}

void XcbTrayBackend::stop()
{
    if (m_connection == nullptr) {
        return;
    }
    delete m_notifier;
    m_notifier = nullptr;

    // AGENT-CONTRACT: all icon hosts must be retired before stop(); the
    // coordinator owns them and does this. Their save-set entries are the
    // safety net when the whole process dies instead.
    if (m_ownsSelection) {
        xcb_set_selection_owner(m_connection, XCB_NONE, m_selectionAtom,
                                XCB_CURRENT_TIME);
    }
    if (m_ownerWindow != XCB_NONE) {
        xcb_destroy_window(m_connection, m_ownerWindow);
        m_ownerWindow = XCB_NONE;
    }
    freeTrayVisual(m_connection, m_screen, &m_trayVisual);
    xcb_disconnect(m_connection);
    m_connection = nullptr;
    m_screen = nullptr;
    m_ownsSelection = false;
    m_icons.clear();
    m_damageWatches.clear();
}

bool XcbTrayBackend::checkExtensions()
{
    xcb_prefetch_extension_data(m_connection, &xcb_composite_id);
    xcb_prefetch_extension_data(m_connection, &xcb_damage_id);
    xcb_prefetch_extension_data(m_connection, &xcb_xfixes_id);
    xcb_prefetch_extension_data(m_connection, &xcb_shape_id);
    xcb_prefetch_extension_data(m_connection, &xcb_test_id);

    const xcb_query_extension_reply_t *composite =
        xcb_get_extension_data(m_connection, &xcb_composite_id);
    if (composite == nullptr || !composite->present) {
        qCCritical(lcXEmbedTray,
                   "X Composite extension missing; cannot redirect icons");
        return false;
    }
    xcb_composite_query_version_cookie_t compositeCookie =
        xcb_composite_query_version(m_connection, 0, 4);
    free(xcb_composite_query_version_reply(m_connection, compositeCookie,
                                           nullptr));

    const xcb_query_extension_reply_t *damage =
        xcb_get_extension_data(m_connection, &xcb_damage_id);
    if (damage == nullptr || !damage->present) {
        qCCritical(lcXEmbedTray,
                   "X Damage extension missing; cannot track icon repaints");
        return false;
    }
    m_haveDamage = true;
    m_damageEventBase = damage->first_event;
    xcb_damage_query_version_cookie_t damageCookie =
        xcb_damage_query_version(m_connection, XCB_DAMAGE_MAJOR_VERSION,
                                 XCB_DAMAGE_MINOR_VERSION);
    free(xcb_damage_query_version_reply(m_connection, damageCookie, nullptr));

    const xcb_query_extension_reply_t *xfixes =
        xcb_get_extension_data(m_connection, &xcb_xfixes_id);
    m_haveXFixes = xfixes != nullptr && xfixes->present;
    if (m_haveXFixes) {
        m_xfixesEventBase = xfixes->first_event;
    } else {
        // Without XFixes an inert proxy never learns the selection was freed;
        // it stays inert until restart. Logged, not fatal.
        qCWarning(lcXEmbedTray,
                  "XFixes missing; inert mode cannot recover the selection");
    }

    const xcb_query_extension_reply_t *shape =
        xcb_get_extension_data(m_connection, &xcb_shape_id);
    m_haveShape = shape != nullptr && shape->present;
    const xcb_query_extension_reply_t *xtest =
        xcb_get_extension_data(m_connection, &xcb_test_id);
    m_haveXTest = xtest != nullptr && xtest->present;
    return true;
}

bool XcbTrayBackend::internAtoms()
{
    const QByteArray selectionName =
        QByteArray("_NET_SYSTEM_TRAY_S") + QByteArray::number(m_screenNumber);
    m_selectionAtom = internAtom(m_connection, selectionName.constData());
    m_opcodeAtom = internAtom(m_connection, "_NET_SYSTEM_TRAY_OPCODE");
    m_messageDataAtom = internAtom(m_connection, "_NET_SYSTEM_TRAY_MESSAGE_DATA");
    m_visualAtom = internAtom(m_connection, "_NET_SYSTEM_TRAY_VISUAL");
    m_orientationAtom = internAtom(m_connection, "_NET_SYSTEM_TRAY_ORIENTATION");
    m_managerAtom = internAtom(m_connection, "MANAGER");
    m_xembedAtom = internAtom(m_connection, "_XEMBED");
    m_xembedInfoAtom = internAtom(m_connection, "_XEMBED_INFO");
    m_netWmNameAtom = internAtom(m_connection, "_NET_WM_NAME");
    m_netWmWindowOpacityAtom =
        internAtom(m_connection, "_NET_WM_WINDOW_OPACITY");
    m_utf8StringAtom = internAtom(m_connection, "UTF8_STRING");
    return m_selectionAtom != XCB_ATOM_NONE && m_opcodeAtom != XCB_ATOM_NONE
        && m_xembedAtom != XCB_ATOM_NONE && m_managerAtom != XCB_ATOM_NONE;
}

void XcbTrayBackend::claimSelection()
{
    if (m_connection == nullptr || m_ownsSelection) {
        return;
    }
    xcb_get_selection_owner_cookie_t cookie =
        xcb_get_selection_owner(m_connection, m_selectionAtom);
    xcb_get_selection_owner_reply_t *reply =
        xcb_get_selection_owner_reply(m_connection, cookie, nullptr);
    if (reply == nullptr) {
        return;
    }
    const xcb_window_t owner = reply->owner;
    free(reply);

    if (owner != XCB_NONE) {
        // AGENT-GUARD: two trays must not fight over the selection. Log the
        // owner and stay inert; XFixes tells us when it goes away.
        qCInfo(lcXEmbedTray,
               "XEmbed tray selection already owned by window 0x%x; inert",
               owner);
        armSelectionWatch();
        return;
    }

    xcb_set_selection_owner(m_connection, m_ownerWindow, m_selectionAtom,
                            XCB_CURRENT_TIME);
    xcb_get_selection_owner_cookie_t verifyCookie =
        xcb_get_selection_owner(m_connection, m_selectionAtom);
    xcb_get_selection_owner_reply_t *verify =
        xcb_get_selection_owner_reply(m_connection, verifyCookie, nullptr);
    const bool claimed =
        verify != nullptr && verify->owner == m_ownerWindow;
    free(verify);
    if (!claimed) {
        qCWarning(lcXEmbedTray, "lost the race for the tray selection; inert");
        armSelectionWatch();
        return;
    }

    m_ownsSelection = true;
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_ownerWindow,
                        m_visualAtom, XCB_ATOM_VISUALID, 32, 1,
                        &m_trayVisual.visualId);
    const quint32 orientation = kHorizontalOrientation;
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_ownerWindow,
                        m_orientationAtom, XCB_ATOM_CARDINAL, 32, 1,
                        &orientation);

    xcb_client_message_event_t manager;
    std::memset(&manager, 0, sizeof(manager));
    manager.response_type = XCB_CLIENT_MESSAGE;
    manager.format = 32;
    manager.window = m_screen->root;
    manager.type = m_managerAtom;
    manager.data.data32[0] = XCB_CURRENT_TIME;
    manager.data.data32[1] = m_selectionAtom;
    manager.data.data32[2] = m_ownerWindow;
    xcb_send_event(m_connection, false, m_screen->root,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&manager));
    xcb_flush(m_connection);
    qCInfo(lcXEmbedTray, "claimed the XEmbed tray selection");
    Q_EMIT selectionClaimed();
}

void XcbTrayBackend::armSelectionWatch()
{
    if (!m_haveXFixes || m_selectionWatchArmed) {
        return;
    }
    xcb_xfixes_select_selection_input(
        m_connection, m_screen->root, m_selectionAtom,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER
            | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY
            | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
    xcb_flush(m_connection);
    m_selectionWatchArmed = true;
}

void XcbTrayBackend::handleSelectionClear()
{
    m_ownsSelection = false;
    armSelectionWatch();
    Q_EMIT selectionLost();
}

TrayIconHost *XcbTrayBackend::createIconHost(quint32 clientWindow)
{
    if (m_connection == nullptr || !m_ownsSelection) {
        return nullptr;
    }
    auto *icon = new XcbEmbeddedIcon(this, clientWindow);
    m_icons.insert(clientWindow, icon);
    return icon;
}

void XcbTrayBackend::unregisterIcon(quint32 clientWindow)
{
    m_icons.remove(clientWindow);
    m_damageWatches.remove(clientWindow);
}

void XcbTrayBackend::addDamageWatch(quint32 clientWindow, quint32 damageId)
{
    m_damageWatches.insert(clientWindow, damageId);
}

void XcbTrayBackend::removeDamageWatch(quint32 clientWindow)
{
    m_damageWatches.remove(clientWindow);
}

xcb_atom_t XcbTrayBackend::atom(Atom which) const
{
    switch (which) {
    case Atom::Selection: return m_selectionAtom;
    case Atom::Opcode: return m_opcodeAtom;
    case Atom::MessageData: return m_messageDataAtom;
    case Atom::Visual: return m_visualAtom;
    case Atom::Orientation: return m_orientationAtom;
    case Atom::Manager: return m_managerAtom;
    case Atom::Xembed: return m_xembedAtom;
    case Atom::XembedInfo: return m_xembedInfoAtom;
    case Atom::NetWmName: return m_netWmNameAtom;
    case Atom::NetWmWindowOpacity: return m_netWmWindowOpacityAtom;
    case Atom::Utf8String: return m_utf8StringAtom;
    }
    return XCB_ATOM_NONE;
}

XcbPixmapFormat XcbTrayBackend::pixmapFormat(quint8 depth) const
{
    const auto it = m_pixmapFormats.constFind(depth);
    if (it != m_pixmapFormats.constEnd()) {
        return it.value();
    }
    return {};
}

QString XcbTrayBackend::readWindowTitle(quint32 window) const
{
    // Never read more than 1 KiB for a title; the SNI boundary bounds again.
    constexpr quint32 kMaxTitleLongs = 256;
    xcb_get_property_cookie_t cookie =
        xcb_get_property(m_connection, false, window, m_netWmNameAtom,
                         m_utf8StringAtom, 0, kMaxTitleLongs);
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(m_connection, cookie, nullptr);
    QString title;
    if (reply != nullptr && xcb_get_property_value_length(reply) > 0) {
        title = QString::fromUtf8(
            static_cast<const char *>(xcb_get_property_value(reply)),
            xcb_get_property_value_length(reply));
    }
    free(reply);
    if (title.isEmpty()) {
        cookie = xcb_get_property(m_connection, false, window,
                                  XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0,
                                  kMaxTitleLongs);
        reply = xcb_get_property_reply(m_connection, cookie, nullptr);
        if (reply != nullptr && xcb_get_property_value_length(reply) > 0) {
            title = QString::fromLocal8Bit(
                static_cast<const char *>(xcb_get_property_value(reply)),
                xcb_get_property_value_length(reply));
        }
        free(reply);
    }
    return title;
}

void XcbTrayBackend::drainEvents()
{
    // AGENT-GUARD: a dead X server (XWayland exits with its session) leaves
    // this descriptor readable at EOF forever. xcb_poll_for_event() then
    // returns nullptr immediately, so the drain below consumes nothing and the
    // still-enabled notifier refires on every event-loop pass - a permanent
    // 100% CPU spin. Orphans were observed burning a full core for hours after
    // their session ended. Disable the notifier and let main() exit; the
    // process cannot proxy anything without a display.
    if (m_connection == nullptr || xcb_connection_has_error(m_connection) != 0) {
        if (m_notifier != nullptr) {
            m_notifier->setEnabled(false);
        }
        Q_EMIT displayLost();
        return;
    }
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(m_connection)) != nullptr) {
        // Strip the "sent by SendEvent" marker bit (what xcb-util's
        // XCB_EVENT_RESPONSE_TYPE does) without an xcb-util dependency.
        const quint8 responseType = event->response_type & quint8(0x7F);
        if (responseType == XCB_CLIENT_MESSAGE) {
            handleClientMessage(
                reinterpret_cast<xcb_client_message_event_t *>(event));
        } else if (responseType == XCB_SELECTION_CLEAR) {
            const auto *clear =
                reinterpret_cast<xcb_selection_clear_event_t *>(event);
            if (clear->selection == m_selectionAtom) {
                handleSelectionClear();
            }
        } else if (responseType == XCB_DESTROY_NOTIFY) {
            const auto *destroy =
                reinterpret_cast<xcb_destroy_notify_event_t *>(event);
            XcbEmbeddedIcon *icon = m_icons.value(destroy->window, nullptr);
            if (icon != nullptr) {
                icon->onDestroyNotify();
            }
        } else if (responseType == XCB_UNMAP_NOTIFY) {
            const auto *unmap =
                reinterpret_cast<xcb_unmap_notify_event_t *>(event);
            XcbEmbeddedIcon *icon = m_icons.value(unmap->window, nullptr);
            if (icon != nullptr) {
                icon->onUnmapNotify();
            }
        } else if (responseType == XCB_REPARENT_NOTIFY) {
            const auto *reparent =
                reinterpret_cast<xcb_reparent_notify_event_t *>(event);
            XcbEmbeddedIcon *icon = m_icons.value(reparent->window, nullptr);
            if (icon != nullptr) {
                icon->onReparentNotify(reparent->parent);
            }
        } else if (responseType == XCB_CONFIGURE_REQUEST) {
            const auto *configure =
                reinterpret_cast<xcb_configure_request_event_t *>(event);
            XcbEmbeddedIcon *icon = m_icons.value(configure->window, nullptr);
            if (icon != nullptr) {
                icon->onConfigureRequest(configure->width, configure->height);
            }
        } else if (responseType == XCB_PROPERTY_NOTIFY) {
            const auto *property =
                reinterpret_cast<xcb_property_notify_event_t *>(event);
            XcbEmbeddedIcon *icon = m_icons.value(property->window, nullptr);
            if (icon != nullptr) {
                icon->onPropertyNotify(property->atom);
            }
        } else if (m_haveDamage
                   && responseType == m_damageEventBase + XCB_DAMAGE_NOTIFY) {
            const auto *damage =
                reinterpret_cast<xcb_damage_notify_event_t *>(event);
            XcbEmbeddedIcon *icon = m_icons.value(damage->drawable, nullptr);
            if (icon != nullptr) {
                icon->onDamageNotify();
                const quint32 watch = m_damageWatches.value(damage->drawable, 0);
                if (watch != 0) {
                    xcb_damage_subtract(m_connection, watch, XCB_NONE,
                                        XCB_NONE);
                }
            }
        } else if (m_haveXFixes
                   && responseType
                       == m_xfixesEventBase + XCB_XFIXES_SELECTION_NOTIFY) {
            const auto *notify =
                reinterpret_cast<xcb_xfixes_selection_notify_event_t *>(event);
            if (notify->selection == m_selectionAtom && !m_ownsSelection) {
                if (notify->owner == XCB_NONE) {
                    Q_EMIT selectionFreed();
                }
            }
        }
        free(event);
    }
    xcb_flush(m_connection);
}

void XcbTrayBackend::handleClientMessage(
    const xcb_client_message_event_t *event)
{
    if (event->type != m_opcodeAtom || event->format != 32) {
        return;
    }
    switch (event->data.data32[1]) {
    case kSystemTrayRequestDock:
        if (m_ownsSelection) {
            Q_EMIT dockRequested(event->data.data32[2]);
        }
        break;
    case kSystemTrayBeginMessage:
    case kSystemTrayCancelMessage:
        // Balloon messages are accepted and ignored: the StatusNotifier
        // protocol has no balloon concept to translate them into (ADR-0229).
        qCDebug(lcXEmbedTray, "ignoring tray balloon message from window 0x%x",
                event->window);
        break;
    default:
        break;
    }
}

} // namespace QindaQt::XEmbedTray
