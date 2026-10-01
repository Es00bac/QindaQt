// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "notificationliveevidenceclient.h"
#include "notificationliveworkflow.h"
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <QDBusConnection>
#include <QProcess>
#include <QSet>

namespace QindaQt::Test {
// Owns only fixture-selected authority and ordinary production child processes.
// The actual fork is the sole native state producer; no synthetic receipt or
// presentation decision is exported by this fixture.
class NativePopupFixture final : public QObject {
    Q_OBJECT
public:
    explicit NativePopupFixture(QString row);
    ~NativePopupFixture() override;
    bool start(QString *error);
    bool mapPopup(QJsonObject *evidence, QString *error);
    bool retired(QJsonObject *evidence, QString *error);
    bool submitCritical(QString *error, quint32 *id = nullptr);
    QDBusConnection session;
    QString selectedOwner;
    QString compositorOwner;
    qint64 compositorPid = 0;
    std::unique_ptr<Platform::Compositor::CompositorAttachment> attachment;
    std::unique_ptr<Services::SessionLockState::QtNativeLockTransport> transport;
    std::unique_ptr<Services::SessionLockState::NativeLockStateMonitor> monitor;
    std::unique_ptr<NotificationLiveEvidenceClient> shellEvidence;
    QList<Services::SessionLockState::LockState> states;
    QSet<QString> receiptNonces;
    bool malformedReceipt = false;
    QProcess shell, host, settings;
private Q_SLOTS:
    void receipt(const QString &nonce, bool locked, bool protectedPresentation,
                 const QDBusMessage &message);
private:
    QString m_row;
    QString m_connectionName;
};
}
