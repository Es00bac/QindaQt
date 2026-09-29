// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "qindaqt/session_supervisor/session_optional_child.h"
#include <QDBusConnection>
#include <QTimer>
namespace QindaQt::SessionSupervisor {
// GUI-thread collaborator. A dedicated unique bus owner fences the daemon's
// lifetime, including an owner activated before this supervisor (ADR-0296).
class KeyringSessionLifetime final : public QObject {
public:
    explicit KeyringSessionLifetime(QObject *parent = nullptr);
    ~KeyringSessionLifetime() override;
    void start(const QString &program);
    void stop() noexcept;
    void resetRestartCount() noexcept { child_.resetRestartCount(); }
private:
    void attach();
    OptionalSessionChild child_{"keyring",{}};
    QString connectionName_;
    std::unique_ptr<QDBusConnection> bus_;
    QTimer admission_;
    int attempts_ = 0;
    bool attached_ = false;
};
}
