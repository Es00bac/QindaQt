// SPDX-License-Identifier: GPL-3.0-or-later
#include "voice_button.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QRandomGenerator>

namespace QindaQt::Controllers {
VoiceButton::VoiceButton(QObject *parent) : QObject(parent), m_bus(QDBusConnection::sessionBus()) {
    m_request = QRandomGenerator::global()->generate64() >> 1;
    auto *watch = new QDBusServiceWatcher("org.qindaqt.Voice1", m_bus,
                                        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watch, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &owner) {
            if (!m_owner.isEmpty() && owner != m_owner) {
                m_token.clear(); m_owner.clear(); m_active = false; m_held = false;
            }
        });
}
void VoiceButton::snapshot(std::function<void(QVariantMap)> done) {
    auto call = QDBusMessage::createMethodCall(m_owner.isEmpty() ? "org.qindaqt.Voice1" : m_owner,
                                              "/org/qindaqt/Voice1", "org.qindaqt.Voice1", "GetSnapshot");
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 3000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, done = std::move(done)](auto *w) {
        QDBusPendingReply<QVariantMap> reply = *w;
        if (!reply.isError() && m_owner.isEmpty()) m_owner = w->reply().service();
        const auto payload = reply.isError() ? QVariantMap{} : reply.value();
        w->deleteLater(); done(payload);
    });
}
void VoiceButton::operation(const QString &method, quint64 revision, std::function<void(bool)> done) {
    auto call = QDBusMessage::createMethodCall(m_owner, "/org/qindaqt/Voice1", "org.qindaqt.Voice1", method);
    call.setArguments({QVariant::fromValue<qulonglong>(++m_request), QVariant::fromValue<qulonglong>(revision)});
    auto *watch = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 3000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [done = std::move(done)](auto *w) {
        QDBusPendingReply<QVariantMap> reply = *w;
        const bool ok = !reply.isError() && reply.value().value("status", -1).toInt() == 0;
        w->deleteLater(); done(ok);
    });
}
void VoiceButton::press(const QString &token) {
    if (!m_token.isEmpty() || m_pending) return;
    m_token = token; m_held = true; m_cancel = false; m_pending = true; m_owner.clear();
    snapshot([this, token](QVariantMap payload) {
        if (token != m_token || !m_held) { m_pending = false; m_token.clear(); return; }
        if (payload.value("state").toInt() != 1 || !payload.value("enabled").toBool()) {
            m_pending = false; m_token.clear(); Q_EMIT failed("voice-unavailable"); return;
        }
        operation("StartDictation", payload.value("revision").toULongLong(), [this, token](bool ok) {
            m_pending = false;
            if (token != m_token) return;
            m_active = ok;
            if (!ok) { m_token.clear(); Q_EMIT failed("voice-start-failed"); return; }
            if (!m_held) finish();
        });
    });
}
void VoiceButton::release(const QString &token, bool cancelRequested) {
    if (token != m_token) return;
    m_held = false; m_cancel = m_cancel || cancelRequested;
    if (m_active && !m_pending) finish();
    else if (!m_pending) m_token.clear();
}
void VoiceButton::cancel() { if (!m_token.isEmpty()) release(m_token, true); }
void VoiceButton::finish() {
    if (!m_active || m_pending) return;
    m_pending = true;
    const auto token = m_token;
    snapshot([this, token](QVariantMap payload) {
        if (token != m_token) { m_pending = false; return; }
        if (payload.isEmpty()) { m_pending = false; m_active = false; m_token.clear(); return; }
        operation(m_cancel ? "Cancel" : "Finish", payload.value("revision").toULongLong(), [this, token](bool ok) {
            if (token != m_token) return;
            m_pending = false; m_active = false; m_token.clear(); m_owner.clear();
            if (!ok) Q_EMIT failed("voice-finish-failed");
        });
    });
}
} // namespace QindaQt::Controllers
