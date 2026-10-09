// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QTimer>
#include <memory>
namespace QindaQt::SessionSupervisor {
// Same-thread selected-session D-Bus lifetime. The backend is activated once
// through its installed D-Bus/systemd service; this class never spawns a
// competing child. The dedicated selected connection retires native authority
// on session stop. Missing backend/display never blocks login; replies are
// transport acknowledgements, not display/privacy admission.
class PortalSessionLifetime final : public QObject {
    Q_OBJECT
public:
    explicit PortalSessionLifetime(QObject *parent = nullptr);
    PortalSessionLifetime(bool captureControl, QObject *parent);
    ~PortalSessionLifetime() override;
    void start(const QString &program, const QString &display);
    // Watch compositor-owned capture broker; never launch or activate it.
    void attachCapture(const QString &display);
    void stop() noexcept;
private:
    void begin(const QString &program, const QString &display);
    void attach();
    std::unique_ptr<QDBusConnection> bus_;
    std::unique_ptr<QDBusServiceWatcher> watcher_;
    QTimer retry_;
    QString service_;
    QString path_;
    QString connectionName_;
    QString display_;
    QString owner_;
    int attempts_ = 0;
    quint64 generation_ = 0;
    bool pending_ = false;
};
}
