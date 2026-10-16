// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shelltaskfacts.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QVector>

#include <memory>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor {
class ContainerControlBridge;
}

namespace QindaQt::Compositor::KWinIntegration {

class CoalescedChangeNotifier;
class ForeignIdentityResolver;
class KWinHybridSession;
class KWinOutputInventory;
class KWinShellVisibilityPublisher;
class KWinShellPanelOwnerSource;
class ManagedWindowRegistry;

// GUI-thread adapter that samples all facts, referenced inventories, and
// container lineage without event-loop reentry, then publishes one immutable
// generation. KWin pointers never cross this boundary.
//
// AGENT-CONTRACT (ADR-0383): sampling is rate-limited to RefreshIntervalMs
// with a trailing flush, and snapshotJson() serves the cached, already
// encoded generation without re-sampling. snapshotChanged() therefore fires
// at most once per interval, and only when the published bytes changed.
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
                                KWinShellPanelOwnerSource &panelOwner,
                                QObject *parent = nullptr);
    ~KWinShellTaskFactsPublisher() override;

    [[nodiscard]] const QByteArray &snapshotJson() override;
    [[nodiscard]] quint64 sampleCount() const noexcept { return m_samples; }

    static constexpr qint64 RefreshIntervalMilliseconds = 250;

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
    KWinShellPanelOwnerSource &m_panelOwner;
    ShellTaskFactsStore m_store;
    // ADR-0169/ADR-0230/ADR-0384: the program behind an opaque launcher class,
    // read once per window; Steam names and PE icons come from a worker.
    std::unique_ptr<ForeignIdentityResolver> m_foreignIdentity;
    std::unique_ptr<CoalescedChangeNotifier> m_refresh;
    QHash<KWin::Window *, QVector<QMetaObject::Connection>> m_connections;
    quint64 m_samples = 0;
};

} // namespace QindaQt::Compositor::KWinIntegration
