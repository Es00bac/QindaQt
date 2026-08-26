// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid/windowtopology.h"

#include <QHash>
#include <QMap>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

using GroupContextApply =
    std::function<void(const QString &containerId, const QString &sourceWindowId)>;
using GroupContextEventSuppression = std::function<bool()>;

// Converts synchronous per-window KWin context signals into one queued,
// container-scoped scene transaction. Dependencies are borrowed and every
// callback runs on the compositor thread.
class KWinGroupContextManager final : public QObject
{
public:
    KWinGroupContextManager(
        ManagedWindowRegistry &registry,
        GroupContextApply apply,
        GroupContextEventSuppression eventsSuppressed = {},
        QObject *parent = nullptr);
    ~KWinGroupContextManager() override;

    KWinGroupContextManager(const KWinGroupContextManager &) = delete;
    KWinGroupContextManager &operator=(const KWinGroupContextManager &) = delete;

    void synchronize(const Hybrid::WindowTopology &topology);
    void shutdown() noexcept;

private:
    void queueMemberContext(const QString &windowId);
    void drainPending();
    void disconnectWindows() noexcept;
    [[nodiscard]] bool eventsAreSuppressed() const;

    ManagedWindowRegistry &m_registry;
    GroupContextApply m_apply;
    GroupContextEventSuppression m_eventsSuppressed;
    QHash<QString, QString> m_containerForWindow;
    QHash<QString, QVector<QMetaObject::Connection>> m_windowConnections;
    QMap<QString, QString> m_pendingSourceByContainer;
    bool m_drainScheduled = false;
    bool m_applying = false;
    bool m_shutdown = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
