// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwincontrolendpoint.h"

#include "kwininputadapter.h"
#include "kwinoutputinventory.h"
#include "kwinshellvisibilitypublisher.h"
#include "layoutgeometry.h"
#include "managedwindowregistry.h"
#include "qindaqt/compositor/containercontrolbridge.h"
#include "qindaqt/compositor/controlcodec.h"
#include "qindaqt/compositor/controlendpoint.h"
#include "qindaqt/compositor/controllimits.h"

#include "windowcontainer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <cmath>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

QByteArray response(QString status, QString code = {}, QString message = {})
{
    QJsonObject object{{QStringLiteral("status"), std::move(status)}};
    if (!code.isEmpty()) {
        object.insert(QStringLiteral("failure"),
                      QJsonObject{{QStringLiteral("code"), std::move(code)},
                                  {QStringLiteral("message"), std::move(message)}});
    }
    return ControlCodec::compactJson(object);
}

} // namespace

KWinControlEndpoint::KWinControlEndpoint(ContainerControlBridge &bridge,
                                         ManagedWindowRegistry &registry,
                                         KWinInputAdapter &inputAdapter,
                                         KWinOutputInventory &outputInventory,
                                         KWinShellVisibilityPublisher &shellVisibility,
                                         bool mutationsEnabled,
                                         bool developmentOutputEnabled,
                                         DevelopmentInputSink *developmentInputSink,
                                         DevelopmentOutputMutator *developmentOutputMutator,
                                         QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
    , m_registry(registry)
    , m_inputAdapter(inputAdapter)
    , m_outputInventory(outputInventory)
    , m_shellVisibility(shellVisibility)
    , m_coreEndpoint(new ControlEndpoint(bridge, this))
    , m_developmentInput(mutationsEnabled, developmentInputSink)
    , m_developmentOutput(developmentOutputEnabled, developmentOutputMutator)
    , m_mutationsEnabled(mutationsEnabled)
{
    connect(m_coreEndpoint, &ControlEndpoint::ContainerCommitted,
            this, &KWinControlEndpoint::ContainerCommitted);
    connect(&registry, &ManagedWindowRegistry::windowsChanged,
            this, &KWinControlEndpoint::WindowsChanged);
    connect(&outputInventory, &KWinOutputInventory::inventoryChanged,
            this, &KWinControlEndpoint::OutputsChanged);
    connect(&inputAdapter, &KWinInputAdapter::capabilitiesChanged,
            this, &KWinControlEndpoint::InputCapabilitiesChanged);
    connect(&shellVisibility, &KWinShellVisibilityPublisher::snapshotChanged,
            this, &KWinControlEndpoint::ShellVisibilityChanged);
}

void KWinControlEndpoint::setHybridDiagnosticsProvider(
    HybridDiagnosticsProvider provider)
{
    m_hybridDiagnostics = std::move(provider);
}

void KWinControlEndpoint::setHybridStateProviders(
    HybridContainersProvider containers,
    HybridSnapshotProvider snapshot)
{
    m_hybridContainers = std::move(containers);
    m_hybridSnapshot = std::move(snapshot);
}

void KWinControlEndpoint::setDevelopmentCompositorReinitializer(
    DevelopmentCompositorReinitializer reinitializer)
{
    m_developmentCompositorReinitializer = std::move(reinitializer);
}

QByteArray KWinControlEndpoint::Capabilities() const
{
    auto capabilities = ControlCodec::capabilities();
    auto methods = capabilities.value(QStringLiteral("methods")).toArray();
    for (const auto &method : {QStringLiteral("Windows"), QStringLiteral("Outputs"),
                               QStringLiteral("InputCapabilities"),
                               QStringLiteral("ShellVisibilitySnapshot"),
                               QStringLiteral("Containers"), QStringLiteral("DockWindows"),
                               QStringLiteral("ReleaseContainer"),
                               QStringLiteral("InjectTestInput"),
                               QStringLiteral("ReinitializeCompositingForTest")}) {
        methods.append(method);
    }
    capabilities.insert(QStringLiteral("methods"), methods);
    auto events = capabilities.value(QStringLiteral("events")).toArray();
    for (const auto &event : {QStringLiteral("WindowsChanged"),
                              QStringLiteral("OutputsChanged"),
                              QStringLiteral("InputCapabilitiesChanged")}) {
        events.append(event);
    }
    events.append(QStringLiteral("ShellVisibilityChanged"));
    capabilities.insert(QStringLiteral("events"), events);
    capabilities.insert(QStringLiteral("kwinAbi"), QStringLiteral(QINDAQT_KWIN_ABI_VERSION));
    capabilities.insert(QStringLiteral("mutationsEnabled"), m_mutationsEnabled);
    capabilities.insert(QStringLiteral("controlMode"),
                        m_mutationsEnabled ? QStringLiteral("development-test")
                                           : QStringLiteral("read-only"));
    capabilities.insert(QStringLiteral("developmentInput"),
                        m_developmentInput.capabilities());
    const auto developmentOutput = m_developmentOutput.capabilities();
    if (developmentOutput.value(QStringLiteral("available")).toBool()) {
        capabilities.insert(QStringLiteral("developmentOutput"), developmentOutput);
        methods.append(QStringLiteral("AddVirtualOutputForTest"));
        methods.append(QStringLiteral("RemoveVirtualOutputForTest"));
        capabilities.insert(QStringLiteral("methods"), methods);
    }
    if (m_hybridDiagnostics) {
        capabilities.insert(QStringLiteral("hybrid"), m_hybridDiagnostics());
    }
    return ControlCodec::compactJson(capabilities);
}

QByteArray KWinControlEndpoint::Windows() const
{
    // AGENT-GUARD: Default plugins and --exit-with-session clients can start in
    // either order. A public inventory read is also a safe reconciliation point.
    m_registry.synchronize();
    return ControlCodec::compactJson(
        {{QStringLiteral("status"), QStringLiteral("ok")},
         {QStringLiteral("windows"), m_registry.windowsJson()}});
}

QByteArray KWinControlEndpoint::Outputs() const
{
    return m_outputInventory.responseJson();
}

QByteArray KWinControlEndpoint::InputCapabilities() const
{
    return ControlCodec::compactJson(m_inputAdapter.capabilitiesJson());
}

QByteArray KWinControlEndpoint::ShellVisibilitySnapshot() const
{
    return m_shellVisibility.snapshotJson();
}

QByteArray KWinControlEndpoint::Containers() const
{
    QJsonArray containers;
    for (const auto &id : m_registry.containerIds()) {
        const auto revision = m_bridge.revision(id);
        if (revision) {
            containers.append(QJsonObject{{QStringLiteral("id"), id},
                                          {QStringLiteral("revision"),
                                           QString::number(*revision)},
                                          {QStringLiteral("authority"),
                                           QStringLiteral("control-bridge")}});
        }
    }
    if (m_hybridContainers) {
        for (const auto &entry : m_hybridContainers()) {
            containers.append(entry);
        }
    }
    return ControlCodec::compactJson(
        {{QStringLiteral("status"), QStringLiteral("ok")},
         {QStringLiteral("containers"), containers}});
}

QByteArray KWinControlEndpoint::DockWindows(const QString &targetWindowId,
                                            const QString &incomingWindowId,
                                            const QString &orientation,
                                            const QString &position,
                                            double ratio)
{
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"), QStringLiteral("control-disabled"),
                        QStringLiteral("external compositor mutations are disabled"));
    }
    if (targetWindowId.isEmpty() || incomingWindowId.isEmpty()) {
        return response(QStringLiteral("rejected"), QStringLiteral("malformed-dock-request"),
                        QStringLiteral("target and incoming window IDs must be non-empty"));
    }
    if (targetWindowId.size() > ControlLimits::MaxIdentifierCharacters
        || incomingWindowId.size() > ControlLimits::MaxIdentifierCharacters) {
        return response(QStringLiteral("rejected"), QStringLiteral("request-too-large"),
                        QStringLiteral("window IDs may not exceed %1 characters")
                            .arg(ControlLimits::MaxIdentifierCharacters));
    }
    if (targetWindowId == incomingWindowId) {
        return response(QStringLiteral("rejected"), QStringLiteral("malformed-dock-request"),
                        QStringLiteral("target and incoming windows must be different"));
    }
    if ((orientation != QStringLiteral("horizontal")
         && orientation != QStringLiteral("vertical"))
        || (position != QStringLiteral("first") && position != QStringLiteral("second"))
        || !std::isfinite(ratio) || ratio <= 0.0 || ratio >= 1.0) {
        return response(QStringLiteral("rejected"), QStringLiteral("malformed-dock-request"),
                        QStringLiteral("orientation, position, or ratio is invalid"));
    }

    m_registry.synchronize();
    if (!m_registry.window(targetWindowId) || !m_registry.window(incomingWindowId)) {
        return response(QStringLiteral("rejected"), QStringLiteral("unknown-window"),
                        QStringLiteral("both docking windows must still be managed"));
    }
    if (!m_registry.owner(targetWindowId).isEmpty()
        || !m_registry.owner(incomingWindowId).isEmpty()) {
        return response(QStringLiteral("rejected"), QStringLiteral("already-owned"),
                        QStringLiteral("both docking windows must be independent"));
    }

    const auto suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Core::WindowContainer container(QStringLiteral("container-%1").arg(suffix));
    QString error;
    if (!container.addPage(QStringLiteral("page-%1").arg(suffix),
                           QStringLiteral("leaf-target-%1").arg(suffix),
                           targetWindowId, &error)) {
        return response(QStringLiteral("rejected"), QStringLiteral("docking-failed"), error);
    }

    // AGENT-GUARD: The singleton is synchronous, unpublished staging. Do not
    // emit, return, yield to the event loop, or assign ownership before the
    // split scene transaction commits. On every failure it is unregistered so
    // callers can never observe a one-window movable group.
    const QJsonObject split{{QStringLiteral("type"), QStringLiteral("split-window")},
                            {QStringLiteral("targetWindowId"), targetWindowId},
                            {QStringLiteral("newWindowId"), incomingWindowId},
                            {QStringLiteral("newLeafNodeId"),
                             QStringLiteral("leaf-incoming-%1").arg(suffix)},
                            {QStringLiteral("splitNodeId"),
                             QStringLiteral("split-%1").arg(suffix)},
                            {QStringLiteral("orientation"), orientation},
                            {QStringLiteral("ratio"), ratio},
                            {QStringLiteral("position"), position}};
    const auto containerId = container.id();
    const ControlRequest request{{},
                                 QStringLiteral("dock-%1").arg(suffix),
                                 containerId,
                                 0,
                                 {split}};
    const auto reply = m_bridge.submitStagedSplit(std::move(container), request);
    if (!reply.committed()) {
        return response(QStringLiteral("rejected"),
                        reply.failure.code.isEmpty() ? QStringLiteral("docking-failed")
                                                     : reply.failure.code,
                        reply.failure.message);
    }

    auto object = ControlCodec::replyToJson(reply);
    object.insert(QStringLiteral("status"), QStringLiteral("docked"));
    return ControlCodec::compactJson(object);
}

QByteArray KWinControlEndpoint::ReleaseContainer(const QString &containerId)
{
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"), QStringLiteral("control-disabled"),
                        QStringLiteral("external compositor mutations are disabled"));
    }
    return releaseContainerForCompositor(containerId);
}

QByteArray KWinControlEndpoint::releaseContainerForCompositor(const QString &containerId)
{
    if (containerId.size() > ControlLimits::MaxIdentifierCharacters) {
        return response(QStringLiteral("rejected"), QStringLiteral("request-too-large"),
                        QStringLiteral("containerId exceeds the identifier limit"));
    }
    const auto snapshot = m_bridge.snapshot(containerId);
    if (!snapshot) {
        return response(QStringLiteral("rejected"), QStringLiteral("unknown-container"),
                        QStringLiteral("unknown container '%1'").arg(containerId));
    }
    QString error;
    const auto container = Core::WindowContainer::fromJson(*snapshot, &error);
    if (!container) {
        return response(QStringLiteral("rejected"), QStringLiteral("invalid-container"), error);
    }
    const auto ids = LayoutGeometryPlanner::windowIds(*container);
    const auto revision = m_bridge.revision(containerId);
    if (!revision) {
        return response(QStringLiteral("rejected"), QStringLiteral("release-failed"),
                        QStringLiteral("container revision disappeared during release"));
    }
    if (!ids.isEmpty()) {
        QVector<QJsonObject> operations;
        operations.reserve(ids.size());
        for (const auto &windowId : ids) {
            operations.append({{QStringLiteral("type"), QStringLiteral("detach-window")},
                               {QStringLiteral("windowId"), windowId}});
        }
        const ControlRequest request{
            {},
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            containerId,
            *revision,
            std::move(operations),
        };
        const auto reply = m_bridge.submit(request);
        if (!reply.committed()) {
            return response(QStringLiteral("rejected"), QStringLiteral("release-failed"),
                            reply.failure.message);
        }
    }
    if (m_bridge.contains(containerId)
        && !m_bridge.unregisterContainer(containerId, &error)) {
        return response(QStringLiteral("rejected"), QStringLiteral("release-failed"), error);
    }
    return response(QStringLiteral("released"));
}

QByteArray KWinControlEndpoint::Snapshot(const QString &containerId) const
{
    if (!m_bridge.contains(containerId) && m_hybridSnapshot) {
        const auto hybrid = m_hybridSnapshot(containerId);
        if (hybrid) {
            const auto protocol =
                ControlCodec::capabilities().value(QStringLiteral("protocol"));
            auto reply = *hybrid;
            reply.insert(QStringLiteral("protocol"), protocol);
            reply.insert(QStringLiteral("containerId"), containerId);
            reply.insert(QStringLiteral("status"), QStringLiteral("ok"));
            reply.insert(QStringLiteral("authority"), QStringLiteral("hybrid-process"));
            return ControlCodec::compactJson(reply);
        }
    }
    return m_coreEndpoint->Snapshot(containerId);
}

QByteArray KWinControlEndpoint::Submit(const QByteArray &requestJson)
{
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"), QStringLiteral("control-disabled"),
                        QStringLiteral("external compositor mutations are disabled"));
    }
    return m_coreEndpoint->Submit(requestJson);
}

QByteArray KWinControlEndpoint::InjectTestInput(const QByteArray &requestJson)
{
    return m_developmentInput.injectTestInput(requestJson);
}

QByteArray KWinControlEndpoint::AddVirtualOutputForTest(
    const QString &name, int width, int height, double scale)
{
    return m_developmentOutput.addVirtualOutputForTest(name, width, height, scale);
}

QByteArray KWinControlEndpoint::RemoveVirtualOutputForTest(const QString &name)
{
    return m_developmentOutput.removeVirtualOutputForTest(name);
}

void KWinControlEndpoint::shutdownDevelopmentOutputs()
{
    m_developmentOutput.shutdown();
}

QByteArray KWinControlEndpoint::ReinitializeCompositingForTest()
{
    // AGENT-GUARD: Like InjectTestInput, this surface is visible on an
    // unauthenticated user bus. Production rejects before consulting runtime
    // state so the method cannot become an availability oracle or mutation.
    if (!m_mutationsEnabled) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("control-disabled"),
                        QStringLiteral("external compositor mutations are disabled"));
    }
    if (!m_developmentCompositorReinitializer
        || !m_developmentCompositorReinitializer()) {
        return response(QStringLiteral("rejected"),
                        QStringLiteral("compositor-reinitialize-unavailable"),
                        QStringLiteral("compositor reinitialization is unavailable"));
    }
    return response(QStringLiteral("scheduled"));
}

} // namespace QindaQt::Compositor::KWinIntegration
