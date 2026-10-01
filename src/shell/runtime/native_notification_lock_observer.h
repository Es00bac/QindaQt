// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <memory>

namespace QindaQt::Shell {
// Shell-private composition of public ordinary attachment and native receipts.
// Owns all watchers; same-thread only. The PID is supervisor-provisioned, never
// accepted from a state payload. A failed start or revoked attachment denies
// disclosure; a replacement compositor requires a new explicit composition.
class NativeNotificationLockObserver final : public QObject {
    Q_OBJECT
public:
    NativeNotificationLockObserver(QDBusConnection bus, qint64 compositorPid,
                                   QString runtimeDirectory, QString socketBasename,
                                   QObject *parent = nullptr);
    ~NativeNotificationLockObserver() override;
    bool start(QString *error = nullptr);
    void stop();
    bool contentMayBeShown() const;
Q_SIGNALS:
    void contentMayBeShownChanged(bool allowed);
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
