// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/session_supervisor/session_optional_child.h"
#include <QDBusConnection>
#include <QPointer>
#include <QTimer>
#include <memory>
#include <optional>
class QDBusPendingCallWatcher;
class QDBusServiceWatcher;
namespace QindaQt::SessionSupervisor {
// Same-thread collaborator. Its dedicated persistent bus connection is the
// daemon's selected session lifetime, including each replacement owner. Bounded
// asynchronous attachment never activates a service or confers display trust;
// the daemon admits the retained native display independently (ADR-0348).
class KeyringSessionLifetime final : public QObject {
public:
    explicit KeyringSessionLifetime(QObject *parent = nullptr);
    ~KeyringSessionLifetime() override;
    void start(const QString &program);
    void stop() noexcept;
    void resetRestartCount() noexcept { child_.resetRestartCount(); }
private:
    std::optional<QString> currentOwner() const;
    void selectOwner(const QString &owner);
    void observeOwner(const QString &advertisedOwner = {});
    void retryUnavailable();
    void attach();
    OptionalSessionChild child_{"keyring",{}};
    QString connectionName_;
    std::unique_ptr<QDBusConnection> bus_;
    std::unique_ptr<QDBusServiceWatcher> watcher_;
    QPointer<QDBusPendingCallWatcher> pending_;
    QTimer admission_;
    QString display_;
    QString owner_;
    QString attachedOwner_;
    quint64 generation_ = 0;
    int attempts_ = 0;
};
}
