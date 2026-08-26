// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridtaskidentitypolicy.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

using TaskTopologyProvider = std::function<const Hybrid::WindowTopology &()>;
using TaskPageActivator = std::function<bool(
    const QString &containerId, const QString &pageId, QString *error)>;
using TaskContainerMinimizer = std::function<bool(
    const QString &containerId, QString *error)>;
using TaskEventSuppression = std::function<bool()>;

// KWin signal adapter for the pure task-identity policy. All callbacks and
// borrowed objects live on the compositor thread and must outlive this manager.
class KWinTaskIdentityManager final : public QObject
{
public:
    KWinTaskIdentityManager(ManagedWindowRegistry &registry,
                            TaskTopologyProvider topology,
                            TaskPageActivator activatePage,
                            TaskContainerMinimizer minimizeContainer,
                            TaskEventSuppression eventsSuppressed = {},
                            QObject *parent = nullptr);
    ~KWinTaskIdentityManager() override;

    KWinTaskIdentityManager(const KWinTaskIdentityManager &) = delete;
    KWinTaskIdentityManager &operator=(const KWinTaskIdentityManager &) = delete;

    [[nodiscard]] bool synchronize(const Hybrid::WindowTopology &topology,
                                   QString *error = nullptr);
    [[nodiscard]] QString primaryWindowId(const QString &containerId) const;
    [[nodiscard]] QVector<TaskContainerIdentity> plans() const { return m_plans; }
    [[nodiscard]] bool ownsTransition() const noexcept { return m_applyDepth > 0; }
    void shutdown() noexcept;

private:
    [[nodiscard]] std::optional<QVector<TaskContainerIdentity>> buildPlans(
        const Hybrid::WindowTopology &topology,
        const QString &preferredActiveWindowId,
        QString *error) const;
    [[nodiscard]] bool eventsAreSuppressed() const;
    void reconnect();
    void handleActivated(KWin::Window *window);
    void handleMinimizedChanged(const QString &windowId);
    void enforceMember(const QString &windowId);
    void applyDecision(const TaskIdentityDecision &decision);
    void warnFailure(QLatin1StringView operation,
                     const QString &windowId,
                     const QString &error) const;

    ManagedWindowRegistry &m_registry;
    TaskTopologyProvider m_topology;
    TaskPageActivator m_activatePage;
    TaskContainerMinimizer m_minimizeContainer;
    TaskEventSuppression m_eventsSuppressed;
    QVector<TaskContainerIdentity> m_plans;
    QHash<QString, QVector<QMetaObject::Connection>> m_windowConnections;
    qsizetype m_applyDepth = 0;
    bool m_shutdown = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
