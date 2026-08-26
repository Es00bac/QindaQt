// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellvisibilitysnapshot.h"

#include <QByteArray>
#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QVector>

#include <memory>
#include <functional>
#include <optional>

namespace KWin {
class LogicalOutput;
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ShellVisibilityRefreshScheduler;
class ManagedWindowRegistry;

class KWinShellVisibilityPublisher final : public QObject
{
    Q_OBJECT

public:
    using HybridMaximizedProvider = std::function<bool(const QString &)>;

    explicit KWinShellVisibilityPublisher(ManagedWindowRegistry &registry,
                                          QObject *parent = nullptr);
    ~KWinShellVisibilityPublisher() override;

    [[nodiscard]] const QByteArray &snapshotJson() const noexcept;
    void setHybridMaximizedProvider(HybridMaximizedProvider provider);
    void invalidate();

Q_SIGNALS:
    void snapshotChanged();

private:
    void trackWindow(KWin::Window *window);
    void forgetWindow(KWin::Window *window);
    void rebuildOutputConnections();
    void scheduleRefresh();
    void handleFailure(const QString &code, const QString &message);
    void refresh();
    [[nodiscard]] std::optional<ShellVisibilitySnapshotCandidate>
    sample(QString *error) const;

    ManagedWindowRegistry &m_registry;
    ShellVisibilitySnapshotStore m_store;
    std::unique_ptr<ShellVisibilityRefreshScheduler> m_scheduler;
    QHash<KWin::Window *, QVector<QMetaObject::Connection>> m_windowConnections;
    QVector<QMetaObject::Connection> m_outputConnections;
    HybridMaximizedProvider m_hybridMaximized;
    bool m_outage = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
