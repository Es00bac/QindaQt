// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_surface_controller.h"

#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/shell/desktop_surface/desktop_icon_layout_store.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace QindaQt::Shell::DesktopSurface {
namespace {

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    const auto errors = component.errors();
    messages.reserve(errors.size());
    for (const auto &error : errors) {
        messages.push_back(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

// The part of `geometry` the panels leave free. Negative depths count as
// none. Depths that would consume a whole axis cannot describe a real layout
// (the solver never produces one), so they fail open to the whole output:
// icons under a panel stay reachable, icons crushed into nothing would not.
QRect workAreaFor(const QRect &geometry, const QMargins &reserved)
{
    const QMargins depth(std::max(0, reserved.left()), std::max(0, reserved.top()),
                         std::max(0, reserved.right()), std::max(0, reserved.bottom()));
    const qint64 horizontal = static_cast<qint64>(depth.left()) + depth.right();
    const qint64 vertical = static_cast<qint64>(depth.top()) + depth.bottom();
    if (horizontal >= geometry.width() || vertical >= geometry.height()) {
        return geometry;
    }
    return geometry.marginsRemoved(depth);
}

QVariantMap rectMap(const QRect &rect)
{
    return QVariantMap{{QStringLiteral("x"), rect.x()},
                       {QStringLiteral("y"), rect.y()},
                       {QStringLiteral("width"), rect.width()},
                       {QStringLiteral("height"), rect.height()}};
}

} // namespace

DesktopSurfaceController::DesktopSurfaceController(QGuiApplication &app,
                                                   QQmlEngine &engine,
                                                   BorrowedFacades facades,
                                                   QObject *parent)
    : QObject(parent), m_app(app), m_engine(engine), m_facades(facades),
      m_layoutStore(std::make_unique<DesktopIconLayoutStore>())
{
  // AGENT-GUARD: output geometry is a live fact. An icon's owning output is
  // derived from it, so a hotplug, a resolution change, or a different primary
  // must reach every surface or icons would be drawn by the wrong output or by
  // none at all.
  connect(&m_app, &QGuiApplication::primaryScreenChanged, this,
          [this] { refreshOutputs(); });
  connect(&m_app, &QGuiApplication::screenAdded, this,
          [this](QScreen *) { refreshOutputs(); });
  connect(&m_app, &QGuiApplication::screenRemoved, this,
          [this](QScreen *) { refreshOutputs(); });
}

DesktopSurfaceController::~DesktopSurfaceController()
{
    for (const auto &connection : std::as_const(m_screenConnections)) {
        disconnect(connection);
    }
    qDeleteAll(m_windows);
}

QVariantList DesktopSurfaceController::outputRects() const
{
    QVariantList rects;
    const auto screens = m_app.screens();
    rects.reserve(screens.size());
    for (const QScreen *screen : screens) {
        const QRect geometry = screen->geometry();
        QVariantMap rect = rectMap(geometry);
        rect.insert(QStringLiteral("name"), screen->name());
        // AGENT-NOTE: depths, not rectangles, cross the ADR-0261 boundary, so
        // the work area is always cut from the same QScreen geometry that
        // places this surface, even where the panel plan adopted the
        // compositor's frame for the same output.
        rect.insert(QStringLiteral("workArea"),
                    rectMap(workAreaFor(geometry, m_reservations.value(screen->name()))));
        rects.append(rect);
    }
    return rects;
}

QString DesktopSurfaceController::primaryOutputName() const
{
    const QScreen *primary = m_app.primaryScreen();
    if (primary != nullptr) {
        return primary->name();
    }
    const auto screens = m_app.screens();
    return screens.isEmpty() ? QString() : screens.constFirst()->name();
}

void DesktopSurfaceController::publishGeometry()
{
    const QVariantList rects = outputRects();
    const QString primary = primaryOutputName();
    for (QQuickWindow *window : std::as_const(m_windows)) {
        window->setProperty("outputRects", rects);
        window->setProperty("primaryOutputName", primary);
    }
}

void DesktopSurfaceController::refreshOutputs()
{
    for (const auto &connection : std::as_const(m_screenConnections)) {
        disconnect(connection);
    }
    m_screenConnections.clear();
    for (QScreen *screen : m_app.screens()) {
        m_screenConnections.append(connect(screen, &QScreen::geometryChanged,
                                           this,
                                           [this](const QRect &) { refreshOutputs(); }));
    }

    publishGeometry();
    if (m_started) {
        reconcile();
    }
}

void DesktopSurfaceController::setOutputReservations(
    const QHash<QString, QMargins> &reservations)
{
    // AGENT-GUARD: the runtime republishes after every accepted panel plan,
    // including the many that only change visibility elsewhere. Assigning
    // `outputRects` always notifies QML, so an unchanged map must stop here
    // or every focus change would re-resolve every icon on every output.
    if (reservations == m_reservations) {
        return;
    }
    m_reservations = reservations;
    publishGeometry();
}

void DesktopSurfaceController::adoptProfile(
    const Profiles::LayoutProfile &profile,
    const Applets::ManifestCatalog &applets,
    const AppletHost::CapabilityPolicy &policy)
{
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    QVariantList inventory;
    inventory.reserve(profile.desktopApplets.size());
    for (const auto &applet : profile.desktopApplets) {
        inventory.append(
            AppletRuntime::AppletInstanceResolver::resolveDesktopBuiltin(
                applet, applets, policy, registry)
                .toVariantMap());
    }
    m_inventory = std::move(inventory);
    // Kept windows adopt the edited inventory in place; windows whose
    // inventory became empty are torn down by the reconcile below. Creation
    // only begins after start() — adoption alone must not map surfaces.
    for (QQuickWindow *window : std::as_const(m_windows)) {
        window->setProperty("applets", m_inventory);
    }
    if (m_started) {
        reconcile();
    }
}

void DesktopSurfaceController::setCustomizationAccess(QObject *access)
{
    m_customizationAccess = access;
    for (QQuickWindow *window : std::as_const(m_windows)) {
        window->setProperty("customizationAccess", QVariant::fromValue(access));
    }
}

void DesktopSurfaceController::start()
{
    m_started = true;
    refreshOutputs();
}

void DesktopSurfaceController::reconcile()
{
    const auto screens = m_app.screens();
    for (auto it = m_windows.begin(); it != m_windows.end();) {
        if (!screens.contains(it.key())) {
            delete it.value();
            it = m_windows.erase(it);
        } else {
            ++it;
        }
    }
    // AGENT-GUARD: strictly additive behavior. A resolved inventory of zero
    // (no `desktop` profile section, or no entry resolving against the
    // manifest catalog) must leave the session with exactly zero
    // desktop-surface windows: existing windows are torn down, and never
    // create a window speculatively here.
    if (m_inventory.isEmpty()) {
        qDeleteAll(m_windows);
        m_windows.clear();
        return;
    }
    for (QScreen *screen : screens) {
        if (!m_windows.contains(screen)) {
            createWindow(screen);
        }
    }
}

void DesktopSurfaceController::createWindow(QScreen *screen)
{
    QQmlComponent component(&m_engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.DesktopSurface"),
                             QStringLiteral("DesktopSurface"));
    if (!component.isReady()) {
        // A broken presentation degrades to the plain wallpaper; it must
        // never fail shell startup. The next reconcile retries creation.
        qWarning().noquote() << "QindaQt shell desktop surface component is"
                                " unavailable; the output keeps plain wallpaper:"
                             << componentErrors(component);
        return;
    }
    QObject *object = component.createWithInitialProperties(
        {{QStringLiteral("applets"), m_inventory},
         {QStringLiteral("access"),
          QVariant::fromValue(m_facades.desktopControlsAccess)},
         {QStringLiteral("launcherAccess"),
          QVariant::fromValue(m_facades.launcherAccess)},
         {QStringLiteral("screenName"), screen->name()},
         {QStringLiteral("layoutStore"),
          QVariant::fromValue(static_cast<QObject *>(m_layoutStore.get()))},
         {QStringLiteral("outputRects"), outputRects()},
         {QStringLiteral("primaryOutputName"), primaryOutputName()}});
    auto *raw = qobject_cast<QQuickWindow *>(object);
    if (!raw) {
        qWarning().noquote()
            << "QindaQt shell desktop surface component did not create a window";
        delete object;
        return;
    }
    raw->setScreen(screen);
    // Not an initial property: a null facade must stay a plain null, and the
    // window keeps the same setProperty path as later replacements.
    raw->setProperty("customizationAccess", QVariant::fromValue(m_customizationAccess));
    raw->setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    auto *layer = LayerShellQt::Window::get(raw);
    if (!layer) {
        delete raw;
        return;
    }
    layer->setWantsToBeOnActiveScreen(false);
    layer->setScreen(screen);
    // AGENT-CONTRACT: KWin LayerShellV1Window maps the exact `desktop` scope
    // to WindowType::Desktop. Any other scope becomes Normal and contaminates
    // task-list, active-window, and shell-visibility facts. The wallpaper
    // controller pins the same contract for its own windows.
    layer->setScope(QStringLiteral("desktop"));
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layer->setAnchors(anchors);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setLayer(LayerShellQt::Window::LayerBackground);
    layer->setExclusiveZone(-1);
    layer->setCloseOnDismissed(false);
    layer->setDesiredSize(QSize(0, 0));
    // AGENT-NOTE: LayerShellQt exposes no within-layer ordering request;
    // same-layer surfaces stack by map order. ShellRuntimeApplication starts
    // this controller after the wallpaper controller has shown its per-output
    // windows, so this surface maps above the wallpaper on every output.
    // Unlike the panel surfaces, the input region stays the FULL surface:
    // selection clicks and the right-click menus land anywhere on it, so no
    // mask is ever set here.
    raw->show();
    m_windows.insert(screen, raw);
}

} // namespace QindaQt::Shell::DesktopSurface
