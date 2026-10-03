// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <QVariantMap>
#include <functional>

namespace QindaQt::Controllers {
// One controller-owned hold capture. Requests use the exact Voice1 owner
// and fresh revision; a release racing startup finishes only our capture.
class VoiceButton final : public QObject {
    Q_OBJECT
public:
    explicit VoiceButton(QObject *parent = nullptr);
    void press(const QString &token);
    void release(const QString &token, bool cancel = false);
    void cancel();
Q_SIGNALS:
    void failed(const QString &reason);
private:
    void snapshot(std::function<void(QVariantMap)> done);
    void operation(const QString &method, quint64 revision, std::function<void(bool)> done);
    void finish();
    QDBusConnection m_bus;
    QString m_token, m_owner;
    quint64 m_request = 0;
    bool m_held = false, m_active = false, m_cancel = false, m_pending = false;
};
} // namespace QindaQt::Controllers
