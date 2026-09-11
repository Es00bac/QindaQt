// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_surface_controller.h"

#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QStringList>

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

} // namespace

DesktopSurfaceController::DesktopSurfaceController(QGuiApplication &app,
                                                   QQmlEngine &engine,
                                                   BorrowedFacades facades,
                                                   QObject *parent)
    : QObject(parent), m_app(app), m_engine(engine), m_facades(facades)
{
}

DesktopSurfaceController::~DesktopSurfaceController()
{
    qDeleteAll(m_windows);
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

void DesktopSurfaceController::start()
{
    m_started = true;
    reconcile();
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
         {QStringLiteral("screenName"), screen->name()}});
    auto *raw = qobject_cast<QQuickWindow *>(object);
    if (!raw) {
        qWarning().noquote()
            << "QindaQt shell desktop surface component did not create a window";
        delete object;
        return;
    }
    raw->setScreen(screen);
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
