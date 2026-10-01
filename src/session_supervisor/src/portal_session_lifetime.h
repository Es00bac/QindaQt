// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/session_supervisor/session_optional_child.h>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <memory>
namespace QindaQt::SessionSupervisor {
// Same-thread optional portal child and selected-session D-Bus lifetime. The
// dedicated connection is retained across backend replacement, then disconnected
// before child teardown so native consent/URI authority retires through the retained owner-loss boundary.
// Missing helper/display or failed admission never blocks login. RPC replies are
// only transport acknowledgements; the backend owns display/privacy admission.
class PortalSessionLifetime final : public QObject {
    Q_OBJECT
public:
    explicit PortalSessionLifetime(QObject *parent = nullptr);
    ~PortalSessionLifetime() override;
    void start(const QString &program, const QString &display);
    void stop() noexcept;
    void resetRestartCount() noexcept { child_.resetRestartCount(); }
private:
    void attach();
    OptionalSessionChild child_;
    std::unique_ptr<QDBusConnection> bus_;
    std::unique_ptr<QDBusServiceWatcher> watcher_;
    QTimer retry_;
    QString connectionName_;
    QString display_;
    QString owner_;
    int attempts_ = 0;
    quint64 generation_ = 0;
    bool pending_ = false;
};
}
