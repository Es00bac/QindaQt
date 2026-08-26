// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>

namespace QindaQt::Compositor {
class ContainerControlBridge;
class ControlEndpoint;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;
class KWinInputAdapter;

class KWinControlEndpoint final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Compositor1")

public:
    KWinControlEndpoint(ContainerControlBridge &bridge,
                        ManagedWindowRegistry &registry,
                        KWinInputAdapter &inputAdapter,
                        bool mutationsEnabled,
                        QObject *parent = nullptr);

    // Process-local compositor policy uses this path during lifecycle
    // reconciliation. It deliberately bypasses only the external D-Bus gate;
    // validation, scene rollback, and ownership rules remain identical.
    [[nodiscard]] QByteArray releaseContainerForCompositor(const QString &containerId);

public Q_SLOTS:
    Q_SCRIPTABLE [[nodiscard]] QByteArray Capabilities() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Windows() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Outputs() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray InputCapabilities() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Containers() const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray DockWindows(const QString &targetWindowId,
                                                      const QString &incomingWindowId,
                                                      const QString &orientation,
                                                      const QString &position,
                                                      double ratio);
    Q_SCRIPTABLE [[nodiscard]] QByteArray ReleaseContainer(const QString &containerId);
    Q_SCRIPTABLE [[nodiscard]] QByteArray Snapshot(const QString &containerId) const;
    Q_SCRIPTABLE [[nodiscard]] QByteArray Submit(const QByteArray &requestJson);

Q_SIGNALS:
    Q_SCRIPTABLE void ContainerCommitted(const QByteArray &eventJson);
    Q_SCRIPTABLE void WindowsChanged();
    Q_SCRIPTABLE void OutputsChanged();
    Q_SCRIPTABLE void InputCapabilitiesChanged();

private:
    ContainerControlBridge &m_bridge;
    ManagedWindowRegistry &m_registry;
    KWinInputAdapter &m_inputAdapter;
    ControlEndpoint *m_coreEndpoint = nullptr;
    bool m_mutationsEnabled = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
