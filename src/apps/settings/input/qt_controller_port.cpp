// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controller_port.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>

namespace QindaQt::Apps::SettingsInput {
QtControllerPort::QtControllerPort(QDBusConnection bus, QObject *parent)
    : ControllerPort(parent), m_bus(std::move(bus)) {
    auto *watch = new QDBusServiceWatcher(QLatin1String(Controllers::Service), m_bus,
                                        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watch, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &) {
            ++m_generation; m_refreshing = false; Q_EMIT unavailable(); refresh();
        });
    m_bus.connect(QLatin1String(Controllers::Service), QLatin1String(Controllers::Object),
                  QLatin1String(Controllers::Interface), "Changed", this, SLOT(changed(qulonglong)));
}
void QtControllerPort::changed(qulonglong revision) { Q_UNUSED(revision); refresh(); }
void QtControllerPort::refresh() {
    if (m_refreshing) return;
    m_refreshing = true;
    const auto generation = m_generation;
    auto call = QDBusMessage::createMethodCall(QLatin1String(Controllers::Service), QLatin1String(Controllers::Object),
                                             QLatin1String(Controllers::Interface), "GetSnapshot");
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 2000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *w) {
        QDBusPendingReply<QString> reply = *w;
        w->deleteLater();
        if (generation != m_generation) return;
        m_refreshing = false;
        if (reply.isError() || reply.value().toUtf8().size() > 256 * 1024) { Q_EMIT unavailable(); return; }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(reply.value().toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) { Q_EMIT unavailable(); return; }
        Q_EMIT snapshotReceived(doc.object());
    });
}
void QtControllerPort::apply(const QString &id, const QJsonObject &patch, quint64 revision) {
    mutate("Apply", {id, QString::fromUtf8(QJsonDocument(patch).toJson(QJsonDocument::Compact)), QVariant::fromValue<qulonglong>(revision)});
}
void QtControllerPort::reset(const QString &id, quint64 revision) {
    mutate("Reset", {id, QVariant::fromValue<qulonglong>(revision)});
}
void QtControllerPort::mutate(const QString &method, const QVariantList &arguments) {
    const auto generation = m_generation;
    auto call = QDBusMessage::createMethodCall(QLatin1String(Controllers::Service), QLatin1String(Controllers::Object),
                                             QLatin1String(Controllers::Interface), method);
    call.setArguments(arguments);
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 3000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *w) {
        QDBusPendingReply<QString> reply = *w;
        w->deleteLater();
        if (generation != m_generation) return;
        const auto result = reply.isError() ? QJsonObject{} : QJsonDocument::fromJson(reply.value().toUtf8()).object();
        Q_EMIT completed(result.value("ok").toBool(), result.value("reason").toString("unavailable"));
        refresh();
    });
}
} // namespace QindaQt::Apps::SettingsInput
