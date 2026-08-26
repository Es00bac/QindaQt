// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridmemberpolicy.h"

#include "qindaqt/hybrid/windowtopology.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace QindaQt::Compositor::KWinIntegration {

class KWinChromeManager;
class KWinHybridSceneFactory;
class ManagedWindowRegistry;

using NativeMemberDetach = std::function<bool(
    const QString &containerId, const QString &windowId, QString *error)>;
using MemberEventSuppression = std::function<bool()>;

// Owns native KWin signal observation and delegates policy decisions to the
// toolkit-neutral controller. All objects and callbacks are borrowed and must
// outlive this compositor-thread manager.
class KWinMemberPolicyManager final : public QObject
{
public:
    KWinMemberPolicyManager(ManagedWindowRegistry &registry,
                            KWinChromeManager &chrome,
                            NativeMemberDetach detach,
                            MemberEventSuppression eventsSuppressed = {},
                            QObject *parent = nullptr);
    ~KWinMemberPolicyManager() override;

    KWinMemberPolicyManager(const KWinMemberPolicyManager &) = delete;
    KWinMemberPolicyManager &operator=(const KWinMemberPolicyManager &) = delete;

    [[nodiscard]] bool synchronize(const Hybrid::WindowTopology &topology,
                                   const KWinHybridSceneFactory &scene,
                                   QString *error = nullptr);
    [[nodiscard]] std::optional<MemberFocusState> focusState() const;
    [[nodiscard]] bool chromeVisible(const QString &containerId) const;
    // Reassert after the chrome manager republishes/re-shows overlays.
    void enforceChromeVisibility() const;
    // Idempotently restores member focus presentation while the current
    // topology and committed scene baseline still agree.
    [[nodiscard]] bool restoreForTopologyMutation(QString *error = nullptr);
    [[nodiscard]] bool restoreForLifecycleMutation(QString *error = nullptr);
    // Phase one of plugin teardown. Idempotently clears temporary focus-mode
    // presentation while the grouped scene baseline is still authoritative,
    // but deliberately leaves lifecycle observers connected for recovery.
    void restorePresentationForShutdown() noexcept;
    // Phase two disconnects only. It must run after scene recovery and must
    // never replay grouped geometry over independently restored clients.
    void shutdown() noexcept;

private:
    class Platform;

    void reconnectGroupedWindows(const QVector<MemberGroupBaseline> &groups);
    [[nodiscard]] bool eventsAreSuppressed() const;
    void handleClosed(const QString &windowId);
    void warnFailure(QLatin1StringView operation,
                     const QString &windowId,
                     const QString &error) const;

    ManagedWindowRegistry &m_registry;
    KWinChromeManager &m_chrome;
    std::unique_ptr<Platform> m_platform;
    std::unique_ptr<HybridMemberPolicy> m_policy;
    MemberEventSuppression m_eventsSuppressed;
    QHash<QString, QVector<QMetaObject::Connection>> m_windowConnections;
    bool m_shutdownPresentationRestored = false;
    bool m_shutdown = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
