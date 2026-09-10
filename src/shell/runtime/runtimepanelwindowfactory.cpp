// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimepanelwindowfactory.h"
#include "runtimepanelappletcompatibility.h"
#include "thememappropagation.h"

#include "audio_applet_controller.h"
#include "bluetooth_applet_controller.h"
#include "launcher_applet_controller.h"
#include "notificationcenterappletaccess.h"
#include "power_applet_controller.h"
#include "qindaqt/shell/global_menu/applet/globalmenuappletaccess.h"
#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"

#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include <qindaqt/panel_blur/panel_surface_blur.h>
#include <QQuickWindow>

#include <QPointer>
#include <QMetaProperty>
#include <QRegion>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QJsonObject>
#include <QStringList>

#include <utility>

namespace QindaQt::Shell {
namespace {

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    const auto errors = component.errors();
    messages.reserve(errors.size());
    for (const auto &qmlError : errors) {
        messages.push_back(qmlError.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

void applyInputBounds(QQuickWindow *window, const QVariant &value)
{
    const QRect bounds = value.toRectF().toAlignedRect()
        .intersected(QRect(QPoint(), window->size()));
    window->setMask(QRegion(bounds));
}

// Mirrors PanelContent's dockMode predicate: a centered bottom panel whose
// resolved applets request dockMode, or a legacy dock panel id. The factory
// uses it to raise the shared task-list controller's presentation bound so a
// dock scrolls instead of truncating rows.
bool isDockPanel(const QVariantMap &panel)
{
    if (panel.value(QStringLiteral("edge")).toString() != QLatin1String("bottom")
        || panel.value(QStringLiteral("alignment")).toString()
               != QLatin1String("center")) {
        return false;
    }
    const QString panelId = panel.value(QStringLiteral("id")).toString();
    if (panelId == QLatin1String("dock")
        || panelId == QLatin1String("smart-shelf")) {
        return true;
    }
    const QVariantList applets =
        panel.value(QStringLiteral("applets")).toList();
    for (const QVariant &value : applets) {
        const QVariantMap applet = value.toMap();
        if (applet.value(QStringLiteral("settings")).toMap().value(
                QStringLiteral("dockMode")).toBool()) {
            return true;
        }
    }
    return false;
}

} // namespace

RuntimePanelWindowFactory::RuntimePanelWindowFactory(QQmlEngine &engine,
                                                     const Profiles::LayoutProfile &profile,
                                                     QVariantMap theme,
                                                     const Applets::ManifestCatalog &applets,
                                                     const AppletHost::CapabilityPolicy &policy,
                                                     NotificationCenterAppletAccess *notificationCenterAccess,
                                                     AudioApplet::AudioAppletController *audioAppletAccess,
                                                     BluetoothApplet::BluetoothAppletController *bluetoothAppletAccess,
                                                     PowerApplet::PowerAppletController *powerAppletAccess,
                                                     Launcher::LauncherAppletController *launcherAppletAccess,
                                                     GlobalMenu::GlobalMenuAppletAccess *globalMenuAppletAccess,
                                                     ShellClipboardApplet::ClipboardAppletController *clipboardAppletAccess,
                                                     ShellTaskListApplet::TaskListAppletController *taskListAppletAccess,
                                                     StatusNotifierApplet::StatusNotifierAppletController *statusNotifierAppletAccess)
    : m_engine(engine)
    , m_theme(std::move(theme))
    , m_notificationCenterAccess(notificationCenterAccess)
    , m_audioAppletAccess(audioAppletAccess)
    , m_bluetoothAppletAccess(bluetoothAppletAccess)
    , m_powerAppletAccess(powerAppletAccess)
    , m_launcherAppletAccess(launcherAppletAccess)
    , m_globalMenuAppletAccess(globalMenuAppletAccess)
    , m_clipboardAppletAccess(clipboardAppletAccess)
    , m_taskListAppletAccess(taskListAppletAccess)
    , m_statusNotifierAppletAccess(statusNotifierAppletAccess)
{
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    for (const auto &panel : profile.panels) {
        QVariantMap resolvedPanel = panel.toVariantMap();
        QVariantList resolvedApplets;
        resolvedApplets.reserve(panel.applets.size());
        for (const auto &applet :
             RuntimePanelAppletCompatibility::normalize(panel.applets)) {
            resolvedApplets.append(
                AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                    applet, panel.edge, applets, policy, registry)
                    .toVariantMap());
        }
        resolvedPanel.insert(QStringLiteral("applets"), resolvedApplets);
        m_panels.insert(panel.id, std::move(resolvedPanel));
    }
}

QJsonArray RuntimePanelWindowFactory::appletEvidence() const
{
    QJsonArray evidence;
    QStringList panelIds = m_panels.keys();
    panelIds.sort();
    for (const QString &panelId : std::as_const(panelIds)) {
        const QVariantList applets =
            m_panels.value(panelId).value(QStringLiteral("applets")).toList();
        for (const QVariant &value : applets) {
            const QVariantMap applet = value.toMap();
            const QVariantMap runtime = applet.value(QStringLiteral("runtime")).toMap();
            evidence.append(QJsonObject{
                {QStringLiteral("panelId"), panelId},
                {QStringLiteral("appletId"),
                 applet.value(QStringLiteral("id")).toString()},
                {QStringLiteral("plugin"),
                 applet.value(QStringLiteral("plugin")).toString()},
                {QStringLiteral("ready"),
                 runtime.value(QStringLiteral("ready")).toBool()},
                {QStringLiteral("entryPoint"),
                 runtime.value(QStringLiteral("entryPoint")).toString()},
            });
        }
    }
    return evidence;
}

RuntimePanelWindowFactory::~RuntimePanelWindowFactory() = default;

void RuntimePanelWindowFactory::setTheme(const QVariantMap &theme)
{
    m_theme = theme;
    propagateThemeMapToWindows(m_theme, m_liveWindows);
}

void RuntimePanelWindowFactory::setDesktopControlsAccess(QObject *access) noexcept
{
    m_desktopControlsAccess = access;
}

void RuntimePanelWindowFactory::setPanelQuickConfig(QObject *access) noexcept
{
    m_panelQuickConfig = access;
}

bool RuntimePanelWindowFactory::ensureComponent(QString *error)
{
    if (m_component && m_component->isReady()) {
        return true;
    }
    if (!m_component) {
        m_component = std::make_unique<QQmlComponent>(&m_engine);
        m_component->loadFromModule(QStringLiteral("QindaQt.Shell.Runtime"),
                                    QStringLiteral("RuntimePanel"));
    }
    if (!m_component->isReady()) {
        if (error != nullptr) {
            *error = QStringLiteral("cannot load the runtime panel QML component: %1")
                         .arg(componentErrors(*m_component));
        }
        return false;
    }
    return true;
}

std::unique_ptr<QQuickWindow> RuntimePanelWindowFactory::createWindow(
    const ShellSurface::PanelSurfaceConfiguration &configuration, QString *error)
{
    const auto panel = m_panels.constFind(configuration.identity.panelId);
    if (panel == m_panels.cend()) {
        if (error != nullptr) {
            *error = QStringLiteral("surface '%1' on '%2' has no selected-profile panel")
                         .arg(configuration.identity.panelId, configuration.identity.outputId);
        }
        return {};
    }
    if (!ensureComponent(error)) {
        return {};
    }

    const QString surfaceId = QStringLiteral("%1@%2")
                                  .arg(configuration.identity.panelId,
                                       configuration.identity.outputId);
    const QVariantMap initialProperties = {
        {QStringLiteral("panel"), panel.value()},
        {QStringLiteral("theme"), m_theme},
        {QStringLiteral("surfaceId"), surfaceId},
        {QStringLiteral("notificationCenterAppletAccess"),
         QVariant::fromValue(m_notificationCenterAccess)},
        {QStringLiteral("audioAppletAccess"),
         QVariant::fromValue(m_audioAppletAccess)},
        {QStringLiteral("bluetoothAppletAccess"),
         QVariant::fromValue(m_bluetoothAppletAccess)},
        {QStringLiteral("powerAppletAccess"),
         QVariant::fromValue(m_powerAppletAccess)},
        {QStringLiteral("launcherAppletAccess"),
         QVariant::fromValue(m_launcherAppletAccess)},
        {QStringLiteral("globalMenuAppletAccess"),
         QVariant::fromValue(m_globalMenuAppletAccess)},
        {QStringLiteral("clipboardAppletAccess"),
         QVariant::fromValue(m_clipboardAppletAccess)},
        {QStringLiteral("taskListAppletAccess"),
         QVariant::fromValue(m_taskListAppletAccess)},
        {QStringLiteral("statusNotifierAppletAccess"),
         QVariant::fromValue(m_statusNotifierAppletAccess)},
    };
    QObject *created = m_component->createWithInitialProperties(initialProperties);
    auto *window = qobject_cast<QQuickWindow *>(created);
    if (window == nullptr) {
        delete created;
        if (error != nullptr) {
            const QString details = componentErrors(*m_component);
            *error = details.isEmpty()
                ? QStringLiteral("runtime panel component did not create a QQuickWindow")
                : QStringLiteral("runtime panel creation failed: %1").arg(details);
        }
        return {};
    }

    // AGENT-CONTRACT: LayerShellSurfaceBackend must assign every Wayland role
    // property before the first map. QML therefore keeps this window hidden;
    // showing it here can permanently turn it into an ordinary toplevel.
    if (window->isVisible()) {
        window->hide();
    }
    if (m_desktopControlsAccess != nullptr) {
        // Not an initial property on purpose: RuntimePanel.qml may not declare
        // it yet, and createWithInitialProperties fails hard on unknown names.
        window->setProperty("desktopControlsAccess",
                            QVariant::fromValue(m_desktopControlsAccess));
    }
    if (m_panelQuickConfig != nullptr) {
        window->setProperty("panelQuickConfig",
                            QVariant::fromValue(m_panelQuickConfig));
    }
    window->setObjectName(QStringLiteral("qindaqt-panel-%1").arg(surfaceId));
    // The dock presents every task row (the zone viewport scrolls) instead of
    // truncating at the taskbar cap. The controller is shared shell-wide, so
    // this raise is monotonic and idempotent: a non-dock panel window never
    // lowers it, and today's profiles host one task list instance.
    if (m_taskListAppletAccess != nullptr && isDockPanel(panel.value())) {
        m_taskListAppletAccess->setPresentationLimit(
            QindaQt::ShellTaskListApplet::kMaxPresentedDockEntries);
    }
    if (auto *content = window->findChild<QObject *>(QStringLiteral("runtimePanelContent"));
        content != nullptr) {
        // AGENT-GUARD: a centered dock has transparent solver-allocated
        // margins. Its QWindow mask must track the painted shelf plus hover
        // allowance, otherwise that invisible region blocks desktop input.
        applyInputBounds(window, content->property("inputBounds"));
        auto *blur = new QindaQt::PanelBlur::PanelSurfaceBlur(window);
        blur->attach(window);
        // One effects pass per published QML change: the mask and the blur
        // region consume the same painted bounds, and the blur additionally
        // honors the materialTranslucent truth (quick setting plus the
        // accessibility projection) so it never outlives its material.
        const auto pushEffects = [window, blur, content] {
            applyInputBounds(window, content->property("inputBounds"));
            const bool translucent =
                content->property("materialTranslucent").toBool();
            blur->setRegion(translucent
                ? content->property("inputBounds").toRectF() : QRectF());
        };
        QObject::connect(content, &QObject::destroyed, window, [window] {
            window->setMask(QRegion());
        });
        for (const QString &propertyName :
             {QStringLiteral("inputBounds"),
              QStringLiteral("materialTranslucent")}) {
            const int propertyIndex =
                content->metaObject()->indexOfProperty(propertyName.toLatin1().constData());
            if (propertyIndex >= 0) {
                const QMetaProperty property =
                    content->metaObject()->property(propertyIndex);
                QMetaObject::connect(content, property.notifySignal(), window,
                                     [pushEffects] { pushEffects(); });
            }
        }
        pushEffects();
    }
    m_liveWindows.append(QPointer<QQuickWindow>(window));
    return std::unique_ptr<QQuickWindow>(window);
}

} // namespace QindaQt::Shell
