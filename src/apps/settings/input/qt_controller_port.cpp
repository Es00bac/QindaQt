// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controller_port.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>

namespace QindaQt::Apps::SettingsInput {
QtJsonInputPort::QtJsonInputPort(QDBusConnection bus, QObject *parent, JsonInputEndpoint endpoint)
    : JsonInputPort(parent), m_bus(std::move(bus)), m_endpoint(std::move(endpoint)) {
    if (m_endpoint.service.isEmpty()) m_endpoint = {QLatin1String(Controllers::Service), QLatin1String(Controllers::Object), QLatin1String(Controllers::Interface)};
    auto *watch = new QDBusServiceWatcher(m_endpoint.service, m_bus,
                                        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watch, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &) {
            ++m_generation; m_refreshing = false; m_refreshAgain = false; m_completion.reset();
            Q_EMIT unavailable(); refresh();
        });
    m_bus.connect(m_endpoint.service, m_endpoint.object,
                  m_endpoint.interface, "Changed", this, SLOT(changed(qulonglong)));
}
void QtJsonInputPort::changed(qulonglong revision) { Q_UNUSED(revision); refresh(); }
void QtJsonInputPort::refresh() {
    if (m_refreshing) { m_refreshAgain = true; return; }
    m_refreshing = true;
    const auto generation = m_generation;
    auto call = QDBusMessage::createMethodCall(m_endpoint.service, m_endpoint.object,
                                             m_endpoint.interface, "GetSnapshot");
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 2000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *w) {
        QDBusPendingReply<QString> reply = *w;
        w->deleteLater();
        if (generation != m_generation) return;
        m_refreshing = false;
        if (reply.isError() || reply.value().toUtf8().size() > Controllers::MaxSnapshotBytes) { m_completion.reset(); Q_EMIT unavailable(); return; }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(reply.value().toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) { m_completion.reset(); Q_EMIT unavailable(); return; }
        Q_EMIT snapshotReceived(doc.object());
        // Keep edits gated until a read dispatched after the mutation reply.
        // Changed during an older read must trigger another read, not vanish.
        if (m_refreshAgain) { m_refreshAgain = false; refresh(); return; }
        if (m_completion) {
            const auto result = *m_completion;
            m_completion.reset();
            Q_EMIT completed(result.first, result.second);
        }
    });
}
void QtJsonInputPort::apply(const QString &id, const QJsonObject &patch, quint64 revision) {
    mutate("Apply", {id, QString::fromUtf8(QJsonDocument(patch).toJson(QJsonDocument::Compact)), QVariant::fromValue<qulonglong>(revision)});
}
void QtJsonInputPort::reset(const QString &id, quint64 revision) {
    mutate("Reset", {id, QVariant::fromValue<qulonglong>(revision)});
}
void QtJsonInputPort::run(const QString &id, quint64 revision) { mutate("Run", {id, QVariant::fromValue<qulonglong>(revision)}); }
void QtJsonInputPort::cancelRun() {
    auto call = QDBusMessage::createMethodCall(m_endpoint.service, m_endpoint.object, m_endpoint.interface, "Cancel");
    m_bus.asyncCall(call, 2000);
}
void QtJsonInputPort::mutate(const QString &method, const QVariantList &arguments) {
    const auto generation = m_generation;
    auto call = QDBusMessage::createMethodCall(m_endpoint.service, m_endpoint.object,
                                             m_endpoint.interface, method);
    call.setArguments(arguments);
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 3000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *w) {
        QDBusPendingReply<QString> reply = *w;
        w->deleteLater();
        if (generation != m_generation) return;
        const auto result = reply.isError() ? QJsonObject{} : QJsonDocument::fromJson(reply.value().toUtf8()).object();
        m_completion = qMakePair(result.value("ok").toBool(), result.value("reason").toString("unavailable"));
        refresh();
    });
}
} // namespace QindaQt::Apps::SettingsInput
