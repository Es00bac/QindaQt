// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "voice_configuration_transport.h"
#include <QtCore/QTimer>
namespace QindaQt::Services::VoiceConfiguration {
// Same-thread projection borrowing its transport. No key draft is stored.
// Owner loss or timeout is uncertain and never replays an operation.
class Client final : public QObject {
    Q_OBJECT
public:
    explicit Client(Transport &transport, QObject *parent = nullptr, int operationTimeoutMs = 20000);
    ~Client() override;
    void start();
    void stop();
    void refresh();
    [[nodiscard]] bool save(const QString &key);
    [[nodiscard]] bool reload();
    [[nodiscard]] const Snapshot &snapshot() const { return m_snapshot; }
    [[nodiscard]] bool ready() const { return m_ready; }
    [[nodiscard]] bool busy() const { return m_operationToken != 0; }
    [[nodiscard]] QString status() const { return m_status; }
    [[nodiscard]] QString owner() const { return m_owner; }
Q_SIGNALS:
    void changed();
    void entryClearRequested();
private:
    bool submit(Operation operation, const QString &key);
    void uncertain();
    Transport &m_transport;
    QTimer m_timeout;
    Snapshot m_snapshot;
    QString m_owner, m_status = QStringLiteral("unavailable");
    quint64 m_next = 0, m_fetchToken = 0, m_operationToken = 0, m_request = 0;
    Operation m_operation = Operation::ReloadCredentials;
    bool m_running = false, m_ready = false;
};
}
