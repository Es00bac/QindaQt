// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_controller.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QSet>

namespace QindaQt::Apps::RemovableMedia {
// Freedesktop notification transport only. Exact notification owner and
// attachment mapping fence actions; missing service opens the media window.
class MediaNotifications final : public QObject {
    Q_OBJECT
public:
    MediaNotifications(MediaController &controller, QDBusConnection connection,
                       QObject *parent = nullptr);
private Q_SLOTS:
    void actionInvoked(uint id, const QString &action, const QDBusMessage &message);
    void closed(uint id, uint reason, const QDBusMessage &message);
private:
    void notify(const QString &token, const QString &summary, const QString &body,
                const QStringList &actions);
    void withdraw(const QString &token);
    MediaController &m_controller;
    QDBusConnection m_bus;
    QDBusServiceWatcher m_watcher;
    QMap<uint, QString> m_tokens;
    QMap<QString, quint64> m_sequences;
    QSet<QString> m_pendingTokens;
    QString m_owner;
    quint64 m_epoch = 0;
};
} // namespace QindaQt::Apps::RemovableMedia
