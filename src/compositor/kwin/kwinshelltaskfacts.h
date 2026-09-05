// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shelltaskfacts.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QVector>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor {
class ContainerControlBridge;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinHybridSession;
class KWinOutputInventory;
class KWinShellVisibilityPublisher;
class ManagedWindowRegistry;

// GUI-thread adapter that samples all facts, referenced inventories, and
// container lineage without event-loop reentry, then publishes one immutable
// generation. KWin pointers never cross this boundary.
class KWinShellTaskFactsPublisher final : public QObject,
                                          public ShellTaskFactsSource
{
    Q_OBJECT

public:
    KWinShellTaskFactsPublisher(ManagedWindowRegistry &registry,
                                KWinOutputInventory &outputs,
                                KWinShellVisibilityPublisher &visibility,
                                ContainerControlBridge &bridge,
                                KWinHybridSession &hybrid,
                                QObject *parent = nullptr);
    ~KWinShellTaskFactsPublisher() override;

    [[nodiscard]] const QByteArray &snapshotJson() override;

Q_SIGNALS:
    void snapshotChanged();

private:
    void trackWindow(KWin::Window *window);
    void forgetWindow(KWin::Window *window);
    void scheduleRefresh();
    void refresh();
    [[nodiscard]] std::optional<ShellTaskFactsCandidate> sample(QString *error);

    ManagedWindowRegistry &m_registry;
    KWinOutputInventory &m_outputs;
    KWinShellVisibilityPublisher &m_visibility;
    ContainerControlBridge &m_bridge;
    KWinHybridSession &m_hybrid;
    ShellTaskFactsStore m_store;
    QHash<KWin::Window *, QVector<QMetaObject::Connection>> m_connections;
    bool m_refreshScheduled = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
