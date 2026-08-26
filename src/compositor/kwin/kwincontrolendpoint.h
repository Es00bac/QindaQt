// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "developmentinputprotocol.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <functional>
#include <optional>

namespace QindaQt::Compositor {
class ContainerControlBridge;
class ControlEndpoint;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;
class KWinInputAdapter;
class KWinShellVisibilityPublisher;

class KWinControlEndpoint final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Compositor1")

public:
    using HybridDiagnosticsProvider = std::function<QJsonObject()>;
    using HybridContainersProvider = std::function<QJsonArray()>;
    using HybridSnapshotProvider =
        std::function<std::optional<QJsonObject>(const QString &)>;
    using DevelopmentCompositorReinitializer = std::function<bool()>;

    KWinControlEndpoint(ContainerControlBridge &bridge,
                        ManagedWindowRegistry &registry,
                        KWinInputAdapter &inputAdapter,
                        KWinShellVisibilityPublisher &shellVisibility,
                        bool mutationsEnabled,
                        DevelopmentInputSink *developmentInputSink = nullptr,
                        QObject *parent = nullptr);

    // The provider is invoked synchronously by Capabilities() and must not
    // retain endpoint references. An empty provider omits the optional field.
    void setHybridDiagnosticsProvider(HybridDiagnosticsProvider provider);
    // These providers expose the process-local Hybrid authority through the
    // existing read-only inventory. They must never be reused by Submit or
    // ReleaseContainer, whose external production gate remains authoritative.
    void setHybridStateProviders(HybridContainersProvider containers,
                                 HybridSnapshotProvider snapshot);
    // This callback exists only for isolated nested-session qualification. The
    // public slot rejects production sessions before invoking it.
    void setDevelopmentCompositorReinitializer(
        DevelopmentCompositorReinitializer reinitializer);

    // Process-local compositor policy uses this path during lifecycle
    // reconciliation. It deliberately bypasses only the external D-Bus gate;
    // validation, scene rollback, and ownership rules remain identical.
    [[nodiscard]] QByteArray releaseContainerForCompositor(const QString &containerId);

public Q_SLOTS:
    Q_SCRIPTABLE [[nodiscard]] QByteArray Capabilities() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Windows() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Outputs() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray InputCapabilities() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray ShellVisibilitySnapshot() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Containers() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray DockWindows(const QString &targetWindowId,
                                                      const QString &incomingWindowId,
                                                      const QString &orientation,
                                                      const QString &position,
                                                      double ratio);
    Q_SCRIPTABLE [[nodiscard]] QByteArray ReleaseContainer(const QString &containerId);
    Q_SCRIPTABLE [[nodiscard]] QByteArray Snapshot(const QString &containerId) const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Submit(const QByteArray &requestJson);
    Q_SCRIPTABLE [[nodiscard]] QByteArray InjectTestInput(const QByteArray &requestJson);
    Q_SCRIPTABLE [[nodiscard]] QByteArray ReinitializeCompositingForTest();

Q_SIGNALS:
    Q_SCRIPTABLE void ContainerCommitted(const QByteArray &eventJson);
    Q_SCRIPTABLE void WindowsChanged();
    Q_SCRIPTABLE void OutputsChanged();
    Q_SCRIPTABLE void InputCapabilitiesChanged();
    Q_SCRIPTABLE void ShellVisibilityChanged();

private:
    ContainerControlBridge &m_bridge;
    ManagedWindowRegistry &m_registry;
    KWinInputAdapter &m_inputAdapter;
    KWinShellVisibilityPublisher &m_shellVisibility;
    ControlEndpoint *m_coreEndpoint = nullptr;
    HybridDiagnosticsProvider m_hybridDiagnostics;
    HybridContainersProvider m_hybridContainers;
    HybridSnapshotProvider m_hybridSnapshot;
    DevelopmentCompositorReinitializer m_developmentCompositorReinitializer;
    DevelopmentInputController m_developmentInput;
    bool m_mutationsEnabled = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
