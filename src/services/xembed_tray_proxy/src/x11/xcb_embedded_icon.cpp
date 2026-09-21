// SPDX-License-Identifier: GPL-3.0-or-later

#include "xcb_embedded_icon.h"

#include "xcb_tray_backend.h"
#include "xcb_tray_visual.h"

#include <QtCore/QLoggingCategory>

#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/shape.h>
#include <xcb/xtest.h>

Q_DECLARE_LOGGING_CATEGORY(lcXEmbedTray)

namespace QindaQt::XEmbedTray
{

namespace {

// XEmbed protocol version this proxy speaks (0.1 semantics).
constexpr quint32 kXEmbedVersion = 0;
constexpr quint32 kXEmbedEmbeddedNotify = 0;

constexpr quint32 kEmbedEventMask = XCB_EVENT_MASK_STRUCTURE_NOTIFY
    | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;

// Wine draws its tray icon through the shape extension for transparency; the
// centre of the window is clickable for every real-world icon we have met.
// KDE's proxy scans shape rectangles for the first clickable point instead;
// that refinement is documented in ADR-0229 as future work.

void sendXEmbedMessage(xcb_connection_t *connection, xcb_window_t toWindow,
                       quint32 message, quint32 detail, quint32 data1,
                       quint32 data2, xcb_atom_t xembedAtom)
{
    xcb_client_message_event_t event;
    std::memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = toWindow;
    event.type = xembedAtom;
    event.data.data32[0] = XCB_CURRENT_TIME;
    event.data.data32[1] = message;
    event.data.data32[2] = detail;
    event.data.data32[3] = data1;
    event.data.data32[4] = data2;
    // No event mask: the message is delivered to the client that owns the
    // destination window, which is exactly the embedded application.
    xcb_send_event(connection, false, toWindow, XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char *>(&event));
}

} // namespace

XcbEmbeddedIcon::XcbEmbeddedIcon(XcbTrayBackend *backend, quint32 clientWindow,
                                 QObject *parent)
    : TrayIconHost(parent)
    , m_backend(backend)
    , m_clientWindow(clientWindow)
{
}

XcbEmbeddedIcon::~XcbEmbeddedIcon()
{
    retire();
}

bool XcbEmbeddedIcon::embed()
{
    xcb_connection_t *connection = m_backend->connection();
    const auto client = static_cast<xcb_window_t>(m_clientWindow);

    xcb_get_geometry_cookie_t geometryCookie =
        xcb_get_geometry(connection, client);
    xcb_get_window_attributes_cookie_t attributesCookie =
        xcb_get_window_attributes(connection, client);

    xcb_get_geometry_reply_t *geometry =
        xcb_get_geometry_reply(connection, geometryCookie, nullptr);
    if (geometry == nullptr) {
        return false; // window already gone
    }
    if (geometry->width == 0 || geometry->height == 0) {
        free(geometry);
        return false;
    }
    m_width = qMin<quint32>(geometry->width, TrayIconImage::kMaxDimension);
    m_height = qMin<quint32>(geometry->height, TrayIconImage::kMaxDimension);
    free(geometry);

    xcb_get_window_attributes_reply_t *attributes =
        xcb_get_window_attributes_reply(connection, attributesCookie, nullptr);
    if (attributes == nullptr) {
        return false;
    }
    // Merge our selections into the ones we already hold on this window
    // (your_event_mask), never touching the client's own selections.
    const quint32 clientEvents = attributes->your_event_mask
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    free(attributes);

    const TrayVisual &visual = m_backend->trayVisual();
    xcb_screen_t *screen = m_backend->screen();

    m_containerWindow = xcb_generate_id(connection);
    const quint32 containerMask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL
        | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    const quint32 containerValues[] = {visual.blackPixel, visual.blackPixel,
                                       true, kEmbedEventMask, visual.colormap};
    xcb_create_window(connection, visual.depth, m_containerWindow, screen->root,
                      0, 0, quint16(m_width), quint16(m_height), 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual.visualId,
                      containerMask, containerValues);

    const char windowClass[] = "qindaqt-xembed-tray\0qindaqt-xembed-tray";
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_containerWindow,
                        XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                        quint32(sizeof(windowClass)), windowClass);

    // AGENT-GUARD: opacity 0 must be set before the map so the compositor
    // never presents the container, and the empty input shape makes it
    // click-through. The container exists mapped and positioned because the
    // embedded client only renders into a viewable window, and toolkits
    // sanity-check synthetic click coordinates against window position.
    const quint32 opacity = 0;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_containerWindow,
                        m_backend->atom(XcbTrayBackend::Atom::NetWmWindowOpacity),
                        XCB_ATOM_CARDINAL, 32, 1, &opacity);
    setActiveForInput(false);
    xcb_map_window(connection, m_containerWindow);

    xcb_change_window_attributes(connection, client, XCB_CW_EVENT_MASK,
                                 &clientEvents);
    xcb_change_save_set(connection, XCB_SET_MODE_INSERT, client);
    xcb_reparent_window(connection, client, m_containerWindow, 0, 0);
    xcb_composite_redirect_window(connection, client,
                                  XCB_COMPOSITE_REDIRECT_MANUAL);

    // XEMBED_EMBEDDED_NOTIFY: data1 is the embedder window and data2 the
    // protocol version — the order KDE's proxy ships, which every client
    // toolkit accepts.
    sendXEmbedMessage(connection, client, kXEmbedEmbeddedNotify, 0,
                      m_containerWindow, kXEmbedVersion,
                      m_backend->atom(XcbTrayBackend::Atom::Xembed));

    const quint32 atOrigin[] = {0, 0};
    xcb_configure_window(connection, client,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, atOrigin);
    xcb_map_window(connection, client);
    xcb_clear_area(connection, 0, client, 0, 0, quint16(m_width),
                   quint16(m_height));

    m_damageId = xcb_generate_id(connection);
    xcb_damage_create(connection, m_damageId, client,
                      XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);
    m_backend->addDamageWatch(m_clientWindow, m_damageId);

    // Direct synthetic events only reach clients that selected button input
    // on the icon window or a descendant; anything else gets XTest.
    if (!wantsButtonEvents(client)) {
        m_injectMode = InjectMode::XTest;
    }

    xcb_flush(connection);

    // No damage event covers the first paint, and some clients draw late;
    // hedge with one deferred recapture through the coordinator's coalescing.
    QTimer::singleShot(500, this, [this]() { onDamageNotify(); });

    refreshTitle();
    return true;
}

TrayIconImage XcbEmbeddedIcon::captureIcon()
{
    if (m_retired || m_width == 0 || m_height == 0) {
        return {};
    }
    xcb_connection_t *connection = m_backend->connection();
    xcb_get_image_cookie_t cookie =
        xcb_get_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
                      static_cast<xcb_drawable_t>(m_clientWindow), 0, 0,
                      quint16(m_width), quint16(m_height), 0xFFFFFFFF);
    xcb_get_image_reply_t *reply =
        xcb_get_image_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
        return {};
    }
    const XcbPixmapFormat format = m_backend->pixmapFormat(reply->depth);
    TrayIconImage image = TrayIconImage::fromZPixmap(
        xcb_get_image_data(reply),
        quint32(xcb_get_image_data_length(reply)), m_width, m_height,
        format.strideForWidth(m_width), format.bitsPerPixel, reply->depth,
        m_backend->imageByteOrder());
    free(reply);
    return image;
}

QString XcbEmbeddedIcon::clientTitle() const
{
    return m_title;
}

void XcbEmbeddedIcon::refreshTitle()
{
    QString title = m_backend->readWindowTitle(m_clientWindow);
    if (title == m_title) {
        return;
    }
    m_title = title;
    Q_EMIT clientTitleChanged();
}

bool XcbEmbeddedIcon::wantsButtonEvents(xcb_window_t window) const
{
    xcb_connection_t *connection = m_backend->connection();
    xcb_get_window_attributes_cookie_t attributesCookie =
        xcb_get_window_attributes(connection, window);
    xcb_get_window_attributes_reply_t *attributes =
        xcb_get_window_attributes_reply(connection, attributesCookie, nullptr);
    if (attributes != nullptr) {
        const bool wants =
            (attributes->all_event_masks & XCB_EVENT_MASK_BUTTON_PRESS) != 0;
        const bool blocksPropagation =
            (attributes->do_not_propagate_mask & XCB_EVENT_MASK_BUTTON_PRESS) != 0;
        free(attributes);
        if (wants) {
            return true;
        }
        if (blocksPropagation) {
            return false;
        }
    }
    xcb_query_tree_cookie_t treeCookie = xcb_query_tree(connection, window);
    xcb_query_tree_reply_t *tree =
        xcb_query_tree_reply(connection, treeCookie, nullptr);
    if (tree == nullptr) {
        return false;
    }
    xcb_window_t *children = xcb_query_tree_children(tree);
    const int count = xcb_query_tree_children_length(tree);
    bool found = false;
    for (int i = 0; i < count && !found; ++i) {
        found = wantsButtonEvents(children[i]);
    }
    free(tree);
    return found;
}

void XcbEmbeddedIcon::forwardButton(quint8 button, qint32 rootX, qint32 rootY)
{
    if (m_retired) {
        return;
    }
    xcb_connection_t *connection = m_backend->connection();
    const qint32 clickX = qint32(m_width / 2);
    const qint32 clickY = qint32(m_height / 2);

    // AGENT-NOTE: GTK and Wine sanity-check a synthetic click against the
    // window's position, so the container is first moved under the
    // panel-supplied global point and the X pointer warped into the client.
    // On XWayland the warp moves only the server's pointer bookkeeping, not
    // the user's real cursor.
    quint32 moveTo[2];
    if (button >= 4) {
        xcb_query_pointer_cookie_t pointerCookie = xcb_query_pointer(
            connection, static_cast<xcb_window_t>(m_clientWindow));
        xcb_query_pointer_reply_t *pointer =
            xcb_query_pointer_reply(connection, pointerCookie, nullptr);
        if (pointer != nullptr) {
            moveTo[0] = quint32(qint32(pointer->root_x) - clickX);
            moveTo[1] = quint32(qint32(pointer->root_y) - clickY);
            free(pointer);
        } else {
            moveTo[0] = 0;
            moveTo[1] = 0;
        }
    } else {
        moveTo[0] = quint32(rootX - clickX);
        moveTo[1] = quint32(rootY - clickY);
    }
    xcb_configure_window(connection, m_containerWindow,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, moveTo);

    setActiveForInput(true);
    xcb_warp_pointer(connection, XCB_NONE,
                     static_cast<xcb_window_t>(m_clientWindow), 0, 0, 0, 0,
                     qint16(clickX), qint16(clickY));

    if (m_injectMode == InjectMode::Direct) {
        xcb_button_press_event_t press;
        std::memset(&press, 0, sizeof(press));
        press.response_type = XCB_BUTTON_PRESS;
        press.event = static_cast<xcb_window_t>(m_clientWindow);
        press.time = XCB_CURRENT_TIME;
        press.same_screen = 1;
        press.root = m_backend->screen()->root;
        press.root_x = qint16(rootX);
        press.root_y = qint16(rootY);
        press.event_x = qint16(clickX);
        press.event_y = qint16(clickY);
        press.child = XCB_NONE;
        press.state = 0;
        press.detail = button;
        xcb_send_event(connection, false,
                       static_cast<xcb_window_t>(m_clientWindow),
                       XCB_EVENT_MASK_BUTTON_PRESS,
                       reinterpret_cast<const char *>(&press));

        xcb_button_release_event_t release = press;
        release.response_type = XCB_BUTTON_RELEASE;
        xcb_send_event(connection, false,
                       static_cast<xcb_window_t>(m_clientWindow),
                       XCB_EVENT_MASK_BUTTON_RELEASE,
                       reinterpret_cast<const char *>(&release));
        xcb_flush(connection);
        setActiveForInput(false);
        return;
    }

    xcb_test_fake_input(connection, XCB_BUTTON_PRESS, button, XCB_CURRENT_TIME,
                        static_cast<xcb_window_t>(m_clientWindow),
                        qint16(clickX), qint16(clickY), 0);
    xcb_test_fake_input(connection, XCB_BUTTON_RELEASE, button,
                        XCB_CURRENT_TIME,
                        static_cast<xcb_window_t>(m_clientWindow),
                        qint16(clickX), qint16(clickY), 0);
    xcb_flush(connection);
    // XTest delivery under XWayland's libei path is asynchronous (XWayland →
    // compositor → X), so the input shape must stay open briefly rather than
    // closing in the same flush as the fake events.
    QTimer::singleShot(300, this, [this]() {
        if (!m_retired) {
            setActiveForInput(false);
        }
    });
}

void XcbEmbeddedIcon::setActiveForInput(bool active)
{
    xcb_connection_t *connection = m_backend->connection();
    xcb_rectangle_t rectangle;
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = active ? quint16(m_width) : quint16(0);
    rectangle.height = active ? quint16(m_height) : quint16(0);
    xcb_shape_rectangles(connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, 0,
                         m_containerWindow, 0, 0, 1, &rectangle);
    const quint32 stack[] = {active ? quint32(XCB_STACK_MODE_ABOVE)
                                    : quint32(XCB_STACK_MODE_BELOW)};
    xcb_configure_window(connection, m_containerWindow,
                         XCB_CONFIG_WINDOW_STACK_MODE, stack);
    xcb_flush(connection);
}

void XcbEmbeddedIcon::retire()
{
    if (m_retired) {
        return;
    }
    m_retired = true;
    xcb_connection_t *connection = m_backend->connection();
    if (m_damageId != 0) {
        xcb_damage_destroy(connection, m_damageId);
        m_damageId = 0;
    }
    // AGENT-CONTRACT: retire must reparent the client back to the root
    // window before destroying the container — destroying the container
    // would destroy the client window with it, and a successor tray owner
    // must still find the client alive to dock. (TrayProxyCoordinator's
    // selection-lost path depends on this.)
    const auto client = static_cast<xcb_window_t>(m_clientWindow);
    xcb_change_save_set(connection, XCB_SET_MODE_DELETE, client);
    xcb_unmap_window(connection, client);
    xcb_reparent_window(connection, client, m_backend->screen()->root, 0, 0);
    if (m_containerWindow != XCB_NONE) {
        xcb_destroy_window(connection, m_containerWindow);
        m_containerWindow = XCB_NONE;
    }
    m_backend->unregisterIcon(m_clientWindow);
    xcb_flush(connection);
}

void XcbEmbeddedIcon::onDamageNotify()
{
    if (!m_retired) {
        Q_EMIT iconDamaged();
    }
}

void XcbEmbeddedIcon::onDestroyNotify()
{
    if (!m_retired) {
        Q_EMIT clientGone();
    }
}

void XcbEmbeddedIcon::onUnmapNotify()
{
    if (!m_retired) {
        Q_EMIT clientGone();
    }
}

void XcbEmbeddedIcon::onReparentNotify(quint32 newParent)
{
    if (!m_retired && newParent != m_containerWindow) {
        Q_EMIT clientGone();
    }
}

void XcbEmbeddedIcon::onConfigureRequest(quint16 width, quint16 height)
{
    if (m_retired || width == 0 || height == 0) {
        return;
    }
    // The container redirects its children's configure requests, so the
    // client is not actually resized until we do it; clamp to the icon
    // budget rather than honouring hostile sizes.
    m_width = qMin<quint32>(width, TrayIconImage::kMaxDimension);
    m_height = qMin<quint32>(height, TrayIconImage::kMaxDimension);
    xcb_connection_t *connection = m_backend->connection();
    const quint32 size[] = {m_width, m_height};
    xcb_configure_window(connection, m_containerWindow,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         size);
    xcb_configure_window(connection,
                         static_cast<xcb_window_t>(m_clientWindow),
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         size);
    xcb_flush(connection);
    Q_EMIT iconDamaged();
}

void XcbEmbeddedIcon::onPropertyNotify(quint32 atom)
{
    if (m_retired) {
        return;
    }
    if (atom == m_backend->atom(XcbTrayBackend::Atom::NetWmName)
        || atom == XCB_ATOM_WM_NAME) {
        refreshTitle();
    }
}

} // namespace QindaQt::XEmbedTray
