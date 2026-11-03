// SPDX-License-Identifier: GPL-3.0-or-later
#include "reply_view_model.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace QindaQt::LinkApp {

ReplyViewModel::ReplyViewModel(LinkApi &api, QString device, QString key, QString action, QObject *parent)
    : QObject(parent), m_api(api), m_device(std::move(device)), m_key(std::move(key)), m_action(std::move(action))
{
    connect(&m_api, &LinkApi::replyFinished, this,
            [this](const QString &dev, const QString &k, const QString &act, const QString &error) {
                if (dev != m_device || k != m_key || act != m_action) return;
                if (error.isEmpty()) {
                    Q_EMIT finished();
                } else {
                    fail(error);
                }
            });
}

void ReplyViewModel::load()
{
    m_api.currentNotifications([this](const QString &json, const QString &error) {
        if (!error.isEmpty()) return;
        for (const QJsonValue &v : QJsonDocument::fromJson(json.toUtf8()).array()) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("device")).toString() == m_device && o.value(QStringLiteral("key")).toString() == m_key) {
                m_appName = o.value(QStringLiteral("app")).toString();
                Q_EMIT appNameChanged();
                return;
            }
        }
    });
}

void ReplyViewModel::fail(const QString &code)
{
    m_state = QStringLiteral("error");
    if (code == QLatin1String("not-found") || code == QLatin1String("target-changed")) {
        m_error = tr("That notification is gone, so it can no longer be answered.");
    } else if (code == QLatin1String("not-connected") || code == QLatin1String("unavailable")) {
        m_error = tr("The phone is not connected.");
    } else if (code == QLatin1String("unverified")) {
        m_error = tr("The phone could not confirm that the reply was delivered.");
    } else if (code == QLatin1String("caller-denied")) {
        m_error = tr("This copy of QindaLink isn't allowed to use the QindaLink service. Use the installed app.");
    } else if (code == QLatin1String("denied")) {
        m_error = tr("The phone has not allowed QindaLink to answer notifications.");
    } else {
        m_error = tr("The reply failed (%1).").arg(code);
    }
    Q_EMIT stateChanged();
}

void ReplyViewModel::send(const QString &text)
{
    if (m_state == QLatin1String("sending") || text.trimmed().isEmpty()) return;
    m_state = QStringLiteral("sending");
    m_error.clear();
    Q_EMIT stateChanged();
    m_api.reply(m_device, m_key, m_action, text, [this](const QString &, const QString &error) {
        // "" means the request left for the phone; the phone's answer arrives as ReplyFinished.
        if (!error.isEmpty()) fail(error);
    });
}

} // namespace QindaQt::LinkApp
