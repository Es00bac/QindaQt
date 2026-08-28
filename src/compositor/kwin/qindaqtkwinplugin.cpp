// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqtkwinplugin.h"

#include "kwincontrolendpoint.h"
#include "kwindevelopmentinputinjector.h"
#include "kwindevelopmentoutputseam.h"
#include "kwininputadapter.h"
#include "kwinoutputinventory.h"
#include "kwinhybridsession.h"
#include "kwinsceneadapter.h"
#include "kwinshellvisibilitypublisher.h"
#include "layoutgeometry.h"
#include "managedwindowregistry.h"
#include "mutationcontrol.h"
#include "qindaqt/compositor/containercontrolbridge.h"

#include "windowcontainer.h"

#include <compositor.h>
#include <input.h>
#include <main.h>

#include <QDBusConnectionInterface>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QUuid>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/Compositor";

} // namespace

QindaQtKWinPlugin::QindaQtKWinPlugin()
    : m_mutationsEnabled(mutationsEnabledForCurrentSession())
    , m_developmentVirtualOutputsEnabled(
          developmentVirtualOutputsEnabledForCurrentSession())
    , m_bus(QDBusConnection::sessionBus())
    , m_registry(std::make_unique<ManagedWindowRegistry>())
    , m_outputInventory(std::make_unique<KWinOutputInventory>())
    , m_shellVisibility(std::make_unique<KWinShellVisibilityPublisher>(
          *m_registry, *m_outputInventory))
    , m_inputAdapter(std::make_unique<KWinInputAdapter>(KWin::input()))
    // AGENT-GUARD: Never construct a production-session injector. A null
    // provider makes the process-level absence explicit in addition to the
    // endpoint's gate-before-parse rejection.
    , m_developmentInputInjector(m_mutationsEnabled
                                     ? std::make_unique<KWinDevelopmentInputInjector>(
                                           KWin::input())
                                     : nullptr)
    // AGENT-GUARD: OutputBackend has no capability query. Construct this
    // adapter only when the launcher proved the exact virtual backend; other
    // backends may block in create or be unable to undo the output.
    , m_developmentOutputSeam(
          m_developmentVirtualOutputsEnabled
              ? std::make_unique<KWinDevelopmentOutputSeam>(
                    KWin::kwinApp()->outputBackend())
              : nullptr)
    , m_sceneAdapter(std::make_unique<KWinSceneAdapter>(*m_registry))
    , m_bridge(std::make_unique<ContainerControlBridge>(*m_sceneAdapter))
    , m_endpoint(std::make_unique<KWinControlEndpoint>(
          *m_bridge, *m_registry, *m_inputAdapter, *m_outputInventory,
          *m_shellVisibility,
          m_mutationsEnabled, m_developmentVirtualOutputsEnabled,
          m_developmentInputInjector.get(), m_developmentOutputSeam.get()))
{
    m_hybridSession = std::make_unique<KWinHybridSession>(*m_registry, this);
    m_shellVisibility->setHybridMaximizedProvider([this](const QString &containerId) {
        return m_hybridSession
            && m_hybridSession->isContainerMaximized(containerId);
    });
    connect(m_hybridSession.get(), &KWinHybridSession::shellVisibilityStateChanged,
            m_shellVisibility.get(), &KWinShellVisibilityPublisher::invalidate);
    m_endpoint->setHybridDiagnosticsProvider([this] {
        return m_hybridSession
            ? m_hybridSession->diagnostics()
            : QJsonObject{{QStringLiteral("ready"), false}};
    });
    m_endpoint->setHybridStateProviders(
        [this] {
            return m_hybridSession ? m_hybridSession->publicContainers() : QJsonArray{};
        },
        [this](const QString &containerId) {
            return m_hybridSession
                ? m_hybridSession->publicSnapshot(containerId)
                : std::optional<QJsonObject>{};
        });
    if (m_mutationsEnabled) {
        m_endpoint->setDevelopmentCompositorReinitializer([this] {
            QPointer<KWin::Compositor> compositor = KWin::Compositor::self();
            if (!compositor) {
                return false;
            }
            // Reply to the test request before KWin synchronously tears down
            // the scene and every WindowItem used by Hybrid chrome. The plugin
            // is the timer context: dynamic unload must cancel this DSO-owned
            // functor even when KWin's longer-lived Compositor survives.
            QTimer::singleShot(0, this, [compositor] {
                if (compositor) {
                    compositor->reinitialize();
                }
            });
            return true;
        });
    }
    connect(m_registry.get(), &ManagedWindowRegistry::managedWindowClosed,
            this, &QindaQtKWinPlugin::reconcileClosedWindow, Qt::QueuedConnection);

    m_registeredService = m_bus.registerService(QString::fromLatin1(ServiceName));
    m_registeredObject = m_registeredService
        && m_bus.registerObject(QString::fromLatin1(ObjectPath), m_endpoint.get(),
                                // AGENT-CONTRACT: Only explicitly scriptable
                                // members belong to the versioned D-Bus surface.
                                QDBusConnection::ExportScriptableSlots
                                    | QDBusConnection::ExportScriptableSignals);
    if (!m_registeredObject) {
        qWarning("QindaQt compositor control could not register on the session bus");
    }
}

QindaQtKWinPlugin::~QindaQtKWinPlugin()
{
    // AGENT-GUARD: KWin can unload this binary plugin without terminating the
    // managed clients. Restore every published group while the endpoint,
    // bridge, scene adapter, registry, and KWin windows are all still alive.
    // The copied ID list also prevents ownership mutation from invalidating
    // teardown iteration.
    disconnect(m_registry.get(), nullptr, this, nullptr);
    m_endpoint->setHybridDiagnosticsProvider({});
    m_endpoint->setHybridStateProviders({}, {});
    m_endpoint->setDevelopmentCompositorReinitializer({});
    if (m_hybridSession) {
        m_hybridSession->shutdown();
        m_hybridSession.reset();
    }
    releasePublishedContainers();

    if (m_registeredObject) {
        m_bus.unregisterObject(QString::fromLatin1(ObjectPath));
    }
    if (m_registeredService) {
        m_bus.unregisterService(QString::fromLatin1(ServiceName));
    }
    // Backend removal is synchronous in the launcher-proven VirtualBackend.
    // Unpublish D-Bus first so teardown signals cannot imply a public success.
    m_endpoint->shutdownDevelopmentOutputs();
}

void QindaQtKWinPlugin::releasePublishedContainers()
{
    const auto containerIds = m_registry->containerIds();
    for (const auto &containerId : containerIds) {
        // AGENT-CONTRACT: Lifecycle cleanup is compositor policy, not an
        // external mutation. Calling the process-local path keeps scene
        // rollback and ownership validation but cannot be rejected by the
        // production D-Bus mutation gate.
        const auto document = QJsonDocument::fromJson(
            m_endpoint->releaseContainerForCompositor(containerId));
        const auto result = document.object();
        if (!document.isObject()
            || result.value(QStringLiteral("status")) != QStringLiteral("released")) {
            const auto failure = result.value(QStringLiteral("failure")).toObject();
            qWarning("QindaQt plugin unload could not release container '%s' "
                     "(status='%s', code='%s'): %s",
                     qPrintable(containerId),
                     qPrintable(result.value(QStringLiteral("status")).toString()),
                     qPrintable(failure.value(QStringLiteral("code")).toString()),
                     qPrintable(failure.value(QStringLiteral("message")).toString()));
        }
    }
}

void QindaQtKWinPlugin::reconcileClosedWindow(const QString &windowId,
                                              const QString &containerId)
{
    Q_UNUSED(windowId)
    if (containerId.isEmpty()) {
        return;
    }
    const auto revision = m_bridge->revision(containerId);
    const auto snapshot = m_bridge->snapshot(containerId);
    if (!revision || !snapshot) {
        return;
    }
    QString parseError;
    const auto container = Core::WindowContainer::fromJson(*snapshot, &parseError);
    if (!container) {
        qWarning("QindaQt could not reconcile an invalid container: %s",
                 qPrintable(parseError));
        return;
    }
    QVector<QJsonObject> operations;
    for (const auto &memberId : LayoutGeometryPlanner::windowIds(*container)) {
        if (!m_registry->window(memberId)) {
            operations.append(
                {{QStringLiteral("type"), QStringLiteral("detach-window")},
                 {QStringLiteral("windowId"), memberId}});
        }
    }
    if (operations.isEmpty()) {
        return;
    }
    const ControlRequest request{{},
                                 QUuid::createUuid().toString(QUuid::WithoutBraces),
                                 containerId,
                                 *revision,
                                 std::move(operations)};
    const auto reply = m_bridge->submit(request);
    if (!reply.committed()) {
        qWarning("QindaQt could not reconcile closed container members: %s",
                 qPrintable(reply.failure.message));
        return;
    }
    const auto remaining = Core::WindowContainer::fromJson(reply.snapshot, &parseError);
    if (!remaining) {
        qWarning("QindaQt reconciliation produced an invalid container: %s",
                 qPrintable(parseError));
        return;
    }
    if (!m_bridge->contains(containerId)) {
        return;
    }
    // A container is meaningful only while it groups at least two clients.
    // Release the survivor (or the empty model) through the scene adapter so
    // saved geometry/minimized state is restored before unregistering it.
    if (LayoutGeometryPlanner::windowIds(*remaining).size() <= 1) {
        const auto release = QJsonDocument::fromJson(
                                 m_endpoint->releaseContainerForCompositor(containerId))
                                 .object();
        if (release.value(QStringLiteral("status")) != QStringLiteral("released")) {
            qWarning("QindaQt could not unwrap a closing container");
        }
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
