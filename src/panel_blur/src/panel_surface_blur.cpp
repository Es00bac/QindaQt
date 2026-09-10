// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/panel_blur/panel_surface_blur.h"

#include <QQuickWindow>

#ifndef QINDAQT_PANEL_BLUR_NO_PROTOCOL
#include <QGuiApplication>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtWaylandClient/QWaylandClientExtension>
#include <wayland-client.h>
#include "qwayland-org-kde-kwin-blur.h"
#endif

namespace QindaQt::PanelBlur {

#ifndef QINDAQT_PANEL_BLUR_NO_PROTOCOL

// AGENT-NOTE: the org_kde_kwin_blur manager is a KWin-supported protocol with
// no Qt session integration, so it binds through the generic client-extension
// registry hook. Version 1 (set_region) is all panel translucency needs; the
// destructor-request template argument makes an inactive compositor drop the
// proxy cleanly.
class BlurManagerExtension final
    : public QWaylandClientExtensionTemplate<BlurManagerExtension>,
      public QtWayland::org_kde_kwin_blur_manager
{
public:
    BlurManagerExtension()
        : QWaylandClientExtensionTemplate<BlurManagerExtension>(1)
    {
        initialize();
    }
};

struct PanelSurfaceBlur::BlurObject
{
    explicit BlurObject(BlurManagerExtension *manager,
                        ::wl_surface *surface)
        : wrapper(new QtWayland::org_kde_kwin_blur(manager->blur(surface)))
        , bound(wrapper->isInitialized())
    {
    }

    std::unique_ptr<QtWayland::org_kde_kwin_blur> wrapper;
    bool bound = false;
};

namespace {

[[nodiscard]] ::wl_surface *waylandSurfaceFor(const QQuickWindow *window)
{
    auto *nativeInterface = QGuiApplication::platformNativeInterface();
    return static_cast<::wl_surface *>(nativeInterface->nativeResourceForWindow(
        QByteArrayLiteral("surface"), const_cast<QQuickWindow *>(window)));
}

} // namespace

PanelSurfaceBlur::PanelSurfaceBlur(QObject *parent)
    : QObject(parent)
    , m_manager(std::make_unique<BlurManagerExtension>())
{
    connect(m_manager.get(), &QWaylandClientExtension::activeChanged, this,
            &PanelSurfaceBlur::pushRegion);
}

PanelSurfaceBlur::~PanelSurfaceBlur()
{
    clear();
}

void PanelSurfaceBlur::attach(QQuickWindow *window)
{
    m_window = window;
    m_attached = true;
    pushRegion();
}

void PanelSurfaceBlur::setRegion(const QRectF &bounds)
{
    m_bounds = bounds;
    pushRegion();
}

void PanelSurfaceBlur::clear()
{
    if (m_window && m_manager->isActive()) {
        if (auto *surface = waylandSurfaceFor(m_window); surface != nullptr) {
            m_manager->unset(surface);
        }
    }
    m_blur.reset();
}

bool PanelSurfaceBlur::active() const noexcept
{
    return m_manager->isActive() && m_blur != nullptr && m_blur->bound;
}

QRegion PanelSurfaceBlur::regionForBounds(const QRectF &bounds,
                                          const QSize &windowSize)
{
    if (bounds.isEmpty() || windowSize.isEmpty()) {
        return {};
    }
    const QRect windowRect(QPoint(0, 0), windowSize);
    return QRegion(bounds.toAlignedRect()).intersected(windowRect);
}

void PanelSurfaceBlur::pushRegion()
{
    if (!m_attached || !m_window || !m_manager->isActive()) {
        return;
    }
    auto *surface = waylandSurfaceFor(m_window);
    auto *nativeInterface = QGuiApplication::platformNativeInterface();
    auto *compositor = static_cast<::wl_compositor *>(
        nativeInterface->nativeResourceForIntegration(
            QByteArrayLiteral("compositor")));
    // AGENT-GUARD: a non-Wayland platform (offscreen tests, the preview
    // surface, X11 fallback) has no surface to decorate — blur stays a no-op
    // and the translucent material simply renders unblurred.
    if (surface == nullptr || compositor == nullptr) {
        return;
    }

    const QRegion region = regionForBounds(m_bounds, m_window->size());
    if (region.isEmpty()) {
        clear();
        return;
    }

    if (!m_blur || !m_blur->bound) {
        m_blur = std::make_unique<BlurObject>(m_manager.get(), surface);
    }
    ::wl_region *wlRegion = ::wl_compositor_create_region(compositor);
    if (wlRegion == nullptr) {
        return;
    }
    for (const QRect &rectangle : region) {
        ::wl_region_add(wlRegion, rectangle.x(), rectangle.y(),
                        rectangle.width(), rectangle.height());
    }
    m_blur->wrapper->set_region(wlRegion);
    ::wl_region_destroy(wlRegion);
}

#else

// Build without the Wayland protocol tooling: every request is a stored-state
// no-op so presentation code never needs a platform branch.
struct PanelSurfaceBlur::BlurObject
{
};

PanelSurfaceBlur::PanelSurfaceBlur(QObject *parent)
    : QObject(parent)
    , m_manager(nullptr)
{
}

PanelSurfaceBlur::~PanelSurfaceBlur() = default;

void PanelSurfaceBlur::attach(QQuickWindow *window)
{
    m_window = window;
}

void PanelSurfaceBlur::setRegion(const QRectF &bounds)
{
    m_bounds = bounds;
}

void PanelSurfaceBlur::clear()
{
    m_blur.reset();
}

bool PanelSurfaceBlur::active() const noexcept
{
    return false;
}

QRegion PanelSurfaceBlur::regionForBounds(const QRectF &bounds,
                                          const QSize &windowSize)
{
    if (bounds.isEmpty() || windowSize.isEmpty()) {
        return {};
    }
    return QRegion(bounds.toAlignedRect())
        .intersected(QRect(QPoint(0, 0), windowSize));
}

void PanelSurfaceBlur::pushRegion()
{
}

#endif

} // namespace QindaQt::PanelBlur
