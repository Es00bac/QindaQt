// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/session_supervisor/session_optional_child.h"
#include <QDBusConnection>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>
class QDBusServiceWatcher;
namespace QindaQt::SessionSupervisor {
// Same-thread collaborator. Its dedicated persistent bus connection is the
// daemon's selected session lifetime, including each replacement owner. Bounded
// asynchronous attachment never activates a service or confers display trust;
// the daemon admits the retained native display independently (ADR-0348).
// AGENT-CONTRACT (ADR-0379): every owner/UID lookup made by the admission tick
// is asynchronous; only stop() keeps one bounded blocking owner check before an
// exact-owner Shutdown.
class KeyringSessionLifetime final : public QObject {
public:
    explicit KeyringSessionLifetime(QObject *parent = nullptr);
    ~KeyringSessionLifetime() override;
    void start(const QString &program);
    void stop() noexcept;
    void resetRestartCount() noexcept { child_.resetRestartCount(); }
private:
    // nullopt: transport uncertainty; empty: the name is confirmed absent.
    using OwnerReply = std::function<void(const std::optional<QString> &)>;
    using UidReply = std::function<void(const std::optional<uint> &)>;
    // A reply runs only while `current` still holds (retired attempts drop it).
    using Fence = std::function<bool()>;
    void lookupOwner(Fence current, OwnerReply done);
    void lookupUid(Fence current, const QString &owner, UidReply done);
    std::optional<QString> currentOwnerBlocking() const;
    void selectOwner(const QString &owner);
    void observeOwner(const QString &advertisedOwner = {});
    void observeCurrent(const QString &owner);
    void retryUnavailable();
    void attach();
    void sendAttach(quint64 token, const QString &selected);
    quint64 startChain();
    void cancelChain();
    OptionalSessionChild child_{"keyring",{}};
    QString connectionName_;
    std::unique_ptr<QDBusConnection> bus_;
    std::unique_ptr<QDBusServiceWatcher> watcher_;
    QTimer admission_;
    QString display_;
    QString owner_;
    QString attachedOwner_;
    // chain_ fences one in-flight admission attempt; observations_ fences the
    // latest watcher-triggered owner lookup.
    quint64 chain_ = 0;
    quint64 observations_ = 0;
    int attempts_ = 0;
    bool pending_ = false;
};
}
