// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/voice_configuration/qt_voice_configuration_transport.h>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>
#include <utility>
namespace QindaQt::Services::VoiceConfiguration {
namespace { const QString VoiceName = QStringLiteral("org.qindaqt.Voice1"); }
struct QtTransport::Private {
    explicit Private(QDBusConnection bus) : connection(std::move(bus)) {}
    QDBusConnection connection;
    std::unique_ptr<QDBusServiceWatcher> watcher;
    QString owner;
    quint64 generation = 0;
    bool running = false;
};
QtTransport::QtTransport(QDBusConnection connection, QObject *parent)
    : Transport(parent), d(std::make_unique<Private>(std::move(connection))) {}
QtTransport::~QtTransport() { stop(); }
void QtTransport::start() {
    if (d->running) return;
    d->running = true;
    d->watcher = std::make_unique<QDBusServiceWatcher>(
        VoiceName, d->connection, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(d->watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &owner) {
            if (!d->running) return;
            ++d->generation; setOwner(owner);
        });
    const quint64 generation = d->generation;
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("GetNameOwner"));
    message.setArguments({VoiceName});
    auto *watcher = new QDBusPendingCallWatcher(d->connection.asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
        const QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (!d->running || generation != d->generation) return;
        // AGENT-GUARD: absence is final for this passive discovery. Never call
        // StartServiceByName from the credential extension (ADR0364).
        setOwner(reply.isError() ? QString{} : reply.value());
    });
}
void QtTransport::stop() {
    d->running = false; ++d->generation; setOwner({}); d->watcher.reset();
}
void QtTransport::setOwner(const QString &owner) {
    if (owner == d->owner) return;
    if (!d->owner.isEmpty()) d->connection.disconnect(d->owner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(Interface), QStringLiteral("Changed"),
        this, SLOT(onChanged(quint64,QDBusMessage)));
    d->owner = owner;
    if (!owner.isEmpty()) d->connection.connect(owner, QString::fromLatin1(ObjectPath),
        QString::fromLatin1(Interface), QStringLiteral("Changed"),
        this, SLOT(onChanged(quint64,QDBusMessage)));
    Q_EMIT ownerChanged(owner);
}
void QtTransport::onChanged(quint64, const QDBusMessage &message) {
    if (d->running && !d->owner.isEmpty() && message.service() == d->owner)
        Q_EMIT invalidated(d->owner);
}
void QtTransport::call(const QString &owner, quint64 token, const QString &method,
                       const QVariantList &arguments, bool operation) {
    if (!d->running || owner.isEmpty() || owner != d->owner) {
        if (operation) Q_EMIT operationReply(owner, token, false, {});
        else Q_EMIT snapshotReply(owner, token, false, false, {});
        return;
    }
    const quint64 generation = d->generation;
    auto message = QDBusMessage::createMethodCall(owner, QString::fromLatin1(ObjectPath),
                                                 QString::fromLatin1(Interface), method);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(
        d->connection.asyncCall(message, operation ? 20000 : 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, watcher, owner, token, generation, operation] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            const QDBusMessage wire = watcher->reply();
            watcher->deleteLater();
            if (!d->running || generation != d->generation || owner != d->owner) return;
            const bool good = !reply.isError() && wire.service() == owner;
            const bool unsupported = reply.isError()
                && (reply.error().type() == QDBusError::UnknownInterface
                    || reply.error().type() == QDBusError::UnknownObject
                    || reply.error().type() == QDBusError::UnknownMethod);
            const QVariantMap payload = good ? reply.value() : QVariantMap{};
            if (operation) Q_EMIT operationReply(owner, token, good, payload);
            else Q_EMIT snapshotReply(owner, token, good, unsupported, payload);
        });
}
void QtTransport::fetch(const QString &owner, quint64 token) {
    call(owner, token, QStringLiteral("GetSnapshot"), {}, false);
}
void QtTransport::submit(const QString &owner, quint64 token, quint64 request,
                         quint64 revision, Operation operation, const QString &key) {
    QVariantList arguments{QVariant::fromValue(request), QVariant::fromValue(revision)};
    if (operation == Operation::SaveElevenLabsKey) arguments.append(key);
    call(owner, token, operation == Operation::SaveElevenLabsKey
         ? QStringLiteral("SaveElevenLabsKey") : QStringLiteral("ReloadCredentials"), arguments, true);
}
}
