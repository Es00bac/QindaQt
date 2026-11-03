// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "link_api.h"

#include <QtCore/QObject>

namespace QindaQt::LinkApp {

// The compact reply form opened by a notification's Reply action
// (`qindaqt-link --reply -- <device> <key> <action>`).
//
// States (`state`): "idle", "sending", "error". The form closes itself (`finished`) when the phone
// confirms through `ReplyFinished`.
// AGENT-GUARD: the reply text is personal content; it is handed to Notifications.Reply and never
// logged or kept after the call.
class ReplyViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName NOTIFY appNameChanged FINAL)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
public:
    ReplyViewModel(LinkApi &api, QString device, QString key, QString action, QObject *parent = nullptr);
    [[nodiscard]] QString appName() const { return m_appName; }
    [[nodiscard]] QString state() const { return m_state; }
    [[nodiscard]] QString error() const { return m_error; }

    Q_INVOKABLE void load();
    Q_INVOKABLE void send(const QString &text);

Q_SIGNALS:
    void appNameChanged();
    void stateChanged();
    void finished();

private:
    void fail(const QString &code);

    LinkApi &m_api;
    QString m_device, m_key, m_action;
    QString m_appName;
    QString m_state = QStringLiteral("idle");
    QString m_error;
};

} // namespace QindaQt::LinkApp
