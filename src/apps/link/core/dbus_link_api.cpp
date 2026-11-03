// SPDX-License-Identifier: GPL-3.0-or-later
#include "dbus_link_api.h"

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::LinkApp {
namespace {

constexpr auto kName = "org.qindaqt.Link1";
constexpr auto kRoot = "/org/qindaqt/Link1";
constexpr auto kMain = "org.qindaqt.Link1";
constexpr auto kMessages = "org.qindaqt.Link1.Messages";
constexpr auto kNotifications = "org.qindaqt.Link1.Notifications";
constexpr auto kSetup = "org.qindaqt.Link1.Setup";
constexpr auto kErrorPrefix = "org.qindaqt.Link1.Error.";

// "org.qindaqt.Link1.Error.NotConnected" -> "not-connected".
QString codeFor(const QDBusError &error)
{
    const QString name = error.name();
    if (name.startsWith(QLatin1String(kErrorPrefix))) {
        // The caller policy and the phone both answer Denied; only the message tells them apart
        // (caller_policy.cpp and messages_adaptor.cpp say "caller is not a trusted..." / "no caller authority").
        if (name.endsWith(QLatin1String("Denied")) && (error.message().startsWith(QLatin1String("caller is not"))
                                                       || error.message().startsWith(QLatin1String("no caller")))) {
            return QStringLiteral("caller-denied");
        }
        const QString tail = name.mid(int(qstrlen(kErrorPrefix)));
        QString code;
        for (const QChar c : tail) {
            if (c.isUpper() && !code.isEmpty()) code += QLatin1Char('-');
            code += c.toLower();
        }
        return code;
    }
    if (error.type() == QDBusError::NoReply || error.type() == QDBusError::Timeout) return QStringLiteral("timeout");
    if (error.type() == QDBusError::UnknownMethod || error.type() == QDBusError::UnknownInterface
        || error.type() == QDBusError::UnknownObject) {
        return QStringLiteral("unsupported");
    }
    if (error.type() == QDBusError::ServiceUnknown) return QStringLiteral("unavailable");
    return QStringLiteral("internal");
}

} // namespace

DbusLinkApi::DbusLinkApi(QDBusConnection bus, QObject *parent) : LinkApi(parent), m_bus(std::move(bus))
{
    m_watcher = std::make_unique<QDBusServiceWatcher>(QString::fromLatin1(kName), m_bus,
                                                      QDBusServiceWatcher::WatchForOwnerChange);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) { setAvailable(!owner.isEmpty()); });
    connectSignals();
    if (m_bus.interface() != nullptr) {
        m_available = m_bus.interface()->isServiceRegistered(QString::fromLatin1(kName));
    }
}

void DbusLinkApi::setAvailable(bool available)
{
    if (available == m_available) return;
    m_available = available;
    Q_EMIT availableChanged();
}

void DbusLinkApi::connectSignals()
{
    const QString name = QString::fromLatin1(kName);
    const QString root = QString::fromLatin1(kRoot);
    const QString messagesPath = root + QStringLiteral("/Messages");
    m_bus.connect(name, root, QString::fromLatin1(kMain), QStringLiteral("DevicesChanged"), this,
                  SIGNAL(devicesChanged(QString)));
    m_bus.connect(name, root, QString::fromLatin1(kMain), QStringLiteral("PairingRequested"), this,
                  SIGNAL(pairingRequested(QString, QString, QString, QString)));
    m_bus.connect(name, messagesPath, QString::fromLatin1(kMessages), QStringLiteral("ThreadsChanged"), this,
                  SIGNAL(threadsChanged(QString, QString)));
    m_bus.connect(name, messagesPath, QString::fromLatin1(kMessages), QStringLiteral("SendStatus"), this,
                  SIGNAL(sendStatus(QString, qlonglong, QString, QString, QString)));
    m_bus.connect(name, root, QString::fromLatin1(kNotifications), QStringLiteral("ReplyFinished"), this,
                  SIGNAL(replyFinished(QString, QString, QString, QString)));
}

void DbusLinkApi::call(const QString &interface, const QString &path, const QString &method,
                       const QVariantList &args, Reply done, int timeoutMs)
{
    QDBusMessage message = QDBusMessage::createMethodCall(QString::fromLatin1(kName), path, interface, method);
    message.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, done = std::move(done)] {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (done) done({}, codeFor(watcher->error()));
            return;
        }
        // The service answered, so it is there even if the name watcher has not told us yet.
        setAvailable(true);
        if (done) done(reply.arguments().isEmpty() ? QString() : reply.arguments().constFirst().toString(), {});
    });
}

void DbusLinkApi::devices(Reply done)
{
    call(QString::fromLatin1(kMain), QString::fromLatin1(kRoot), QStringLiteral("Devices"), {}, std::move(done));
}

void DbusLinkApi::pendingPairings(Reply done)
{
    // AGENT-NOTE: PendingPairings arrives with the caller-policy slice. Until then the service answers
    // UnknownMethod, which codeFor maps to "unsupported", and the view model shows live requests only.
    call(QString::fromLatin1(kMain), QString::fromLatin1(kRoot), QStringLiteral("PendingPairings"), {},
         std::move(done));
}

void DbusLinkApi::confirmPairing(const QString &id, bool accept)
{
    call(QString::fromLatin1(kMain), QString::fromLatin1(kRoot), QStringLiteral("ConfirmPairing"), {id, accept}, {});
}

void DbusLinkApi::unpair(const QString &id)
{
    call(QString::fromLatin1(kMain), QString::fromLatin1(kRoot), QStringLiteral("Unpair"), {id}, {});
}

void DbusLinkApi::listThreads(const QString &device, int limit, qint64 before, qint64 beforeId, Reply done)
{
    call(QString::fromLatin1(kMessages), QString::fromLatin1(kRoot) + QStringLiteral("/Messages"),
         QStringLiteral("ListThreads"), {device, limit, qlonglong(before), qlonglong(beforeId)}, std::move(done));
}

void DbusLinkApi::listMessages(const QString &device, qint64 thread, int limit, qint64 before,
                               const QString &beforeKind, qint64 beforeId, Reply done)
{
    call(QString::fromLatin1(kMessages), QString::fromLatin1(kRoot) + QStringLiteral("/Messages"),
         QStringLiteral("ListMessages"),
         {device, qlonglong(thread), limit, qlonglong(before), beforeKind, qlonglong(beforeId)}, std::move(done));
}

void DbusLinkApi::getPart(const QString &device, qint64 part, int maxEdge, Reply done)
{
    call(QString::fromLatin1(kMessages), QString::fromLatin1(kRoot) + QStringLiteral("/Messages"),
         QStringLiteral("GetPart"), {device, qlonglong(part), maxEdge}, std::move(done));
}

void DbusLinkApi::send(const QString &device, const QStringList &addrs, const QString &text, Reply done)
{
    call(QString::fromLatin1(kMessages), QString::fromLatin1(kRoot) + QStringLiteral("/Messages"),
         QStringLiteral("Send"), {device, addrs, text, -1}, std::move(done));
}

void DbusLinkApi::markRead(const QString &device, qint64 thread, Reply done)
{
    call(QString::fromLatin1(kMessages), QString::fromLatin1(kRoot) + QStringLiteral("/Messages"),
         QStringLiteral("MarkRead"), {device, qlonglong(thread)}, std::move(done));
}

void DbusLinkApi::listApps(Reply done)
{
    call(QString::fromLatin1(kNotifications), QString::fromLatin1(kRoot), QStringLiteral("ListApps"), {},
         std::move(done));
}

void DbusLinkApi::setAppMuted(const QString &pkg, bool muted)
{
    call(QString::fromLatin1(kNotifications), QString::fromLatin1(kRoot), QStringLiteral("SetAppMuted"),
         {pkg, muted}, {});
}

void DbusLinkApi::currentNotifications(Reply done)
{
    call(QString::fromLatin1(kNotifications), QString::fromLatin1(kRoot), QStringLiteral("Current"), {},
         std::move(done));
}

void DbusLinkApi::reply(const QString &device, const QString &key, const QString &action, const QString &text,
                        Reply done)
{
    // Reply() returns "" once sent and a protocol code otherwise; fold the code into `error`.
    call(QString::fromLatin1(kNotifications), QString::fromLatin1(kRoot), QStringLiteral("Reply"),
         {device, key, action, text},
         [done = std::move(done)](const QString &result, const QString &error) {
             if (done) done({}, error.isEmpty() ? result : error);
         });
}

void DbusLinkApi::adbDevices(Reply done)
{
    call(QString::fromLatin1(kSetup), QString::fromLatin1(kRoot), QStringLiteral("AdbDevices"), {}, std::move(done));
}

void DbusLinkApi::setupPhone(const QString &serial, Reply done)
{
    call(QString::fromLatin1(kSetup), QString::fromLatin1(kRoot), QStringLiteral("SetupPhone"), {serial},
         std::move(done), 120000);
}

void DbusLinkApi::setupStatus(const QString &serial, Reply done)
{
    call(QString::fromLatin1(kSetup), QString::fromLatin1(kRoot), QStringLiteral("SetupStatus"), {serial},
         std::move(done), 60000);
}

} // namespace QindaQt::LinkApp
