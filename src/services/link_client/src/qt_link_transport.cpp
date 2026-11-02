// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_client/qt_link_transport.h>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Link {
namespace {
constexpr auto kName = "org.qindaqt.Link1";
constexpr auto kPath = "/org/qindaqt/Link1";
}

QtLinkTransport::QtLinkTransport(QDBusConnection bus, QObject *parent) : LinkTransport(parent), m_bus(std::move(bus)) {}
QtLinkTransport::~QtLinkTransport() { stop(); }

bool QtLinkTransport::start(QString *error)
{
    if (m_running) {
        return true;
    }
    if (!m_bus.isConnected() || m_bus.interface() == nullptr) {
        if (error != nullptr) *error = QStringLiteral("link bus connection is unavailable");
        return false;
    }
    m_running = true;
    m_watcher = std::make_unique<QDBusServiceWatcher>(QString::fromLatin1(kName), m_bus,
                                                      QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this, &QtLinkTransport::onOwnerChanged);
    m_bus.connect(QString::fromLatin1(kName), QString::fromLatin1(kPath), QString::fromLatin1(kName),
                  QStringLiteral("DevicesChanged"), this, SLOT(onDevicesChanged(QString)));
    m_bus.connect(QString::fromLatin1(kName), QString::fromLatin1(kPath), QString::fromLatin1(kName),
                  QStringLiteral("PairingRequested"), this, SLOT(onPairingRequested(QString, QString, QString, QString)));
    const QDBusReply<bool> owned = m_bus.interface()->isServiceRegistered(QString::fromLatin1(kName));
    if (owned.isValid() && owned.value()) {
        m_available = true;
        ++m_epoch;
        Q_EMIT serviceAvailableChanged(true);
    }
    return true;
}

void QtLinkTransport::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_bus.disconnect(QString::fromLatin1(kName), QString::fromLatin1(kPath), QString::fromLatin1(kName),
                     QStringLiteral("DevicesChanged"), this, SLOT(onDevicesChanged(QString)));
    m_bus.disconnect(QString::fromLatin1(kName), QString::fromLatin1(kPath), QString::fromLatin1(kName),
                     QStringLiteral("PairingRequested"), this, SLOT(onPairingRequested(QString, QString, QString, QString)));
    m_watcher.reset();
    if (m_available) {
        m_available = false;
        Q_EMIT serviceAvailableChanged(false);
    }
}

void QtLinkTransport::onOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    const bool now = !newOwner.isEmpty();
    if (now == m_available) {
        return;
    }
    m_available = now;
    ++m_epoch;
    Q_EMIT serviceAvailableChanged(now);
}

void QtLinkTransport::requestDevices(quint64 token)
{
    QDBusMessage m = QDBusMessage::createMethodCall(QString::fromLatin1(kName), QString::fromLatin1(kPath),
                                                    QString::fromLatin1(kName), QStringLiteral("Devices"));
    const quint64 epoch = m_epoch;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(m), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, token, epoch](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (epoch != m_epoch) {
            return;  // stale: the service restarted since the request
        }
        if (reply.isError()) {
            Q_EMIT requestFailed(token, QStringLiteral("link request failed: %1").arg(reply.error().message()));
        } else {
            Q_EMIT devicesReceived(token, reply.value().toUtf8());
        }
    });
}

void QtLinkTransport::call(const QString &method, const QVariantList &args)
{
    QDBusMessage m = QDBusMessage::createMethodCall(QString::fromLatin1(kName), QString::fromLatin1(kPath),
                                                    QString::fromLatin1(kName), method);
    m.setArguments(args);
    m_bus.asyncCall(m);
}

void QtLinkTransport::confirmPairing(const QString &id, bool accept) { call(QStringLiteral("ConfirmPairing"), {id, accept}); }
void QtLinkTransport::unpair(const QString &id) { call(QStringLiteral("Unpair"), {id}); }
void QtLinkTransport::ring(const QString &id) { call(QStringLiteral("Ring"), {id}); }

void QtLinkTransport::onDevicesChanged(const QString &json) { Q_EMIT devicesChanged(json.toUtf8()); }
void QtLinkTransport::onPairingRequested(const QString &id, const QString &name, const QString &model, const QString &code)
{
    Q_EMIT pairingRequested(id, name, model, code);
}

} // namespace QindaQt::Link
