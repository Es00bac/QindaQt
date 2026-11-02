// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/notification_presenter.h>

#include <QtCore/QTimer>

namespace QindaQt::Link {

// Test presenter for a private bus: appends one JSON line per event to a file
// instead of talking to a notification server. Lines carry package-free
// facts only: app name, text lengths and a SHA-256 of the summary, never the
// text. Commands for tests are read from "<path>.cmd" every 100 ms:
//   dismiss <handle>   |   action <handle> <key>
// Used only through qindaqt-link-service --recording-notifier FILE.
class RecordingPresenter final : public NotificationPresenter {
    Q_OBJECT
public:
    explicit RecordingPresenter(QString path, QObject *parent = nullptr);
    quint32 present(const PresentedNotification &n, quint32 replaces) override;
    void close(quint32 handle) override;

private:
    void append(const QJsonObject &line) const;
    void pollCommands();

    QString m_path;
    quint32 m_next = 0;
    QTimer m_poll;
};

} // namespace QindaQt::Link
