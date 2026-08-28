// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_service/display_service_ports.h>

#include "display_inventory_validation_p.h"

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::DisplayService
{
namespace
{

constexpr auto kCompositorService = "org.qindaqt.Compositor";
constexpr auto kCompositorPath = "/org/qindaqt/Compositor";
constexpr auto kCompositorInterface = "org.qindaqt.Compositor1";

class QtCompositorInventorySource final : public QObject, public InventorySource
{
    Q_OBJECT

public:
    explicit QtCompositorInventorySource(QDBusConnection connection)
        : m_connection(std::move(connection))
    {
    }

    ~QtCompositorInventorySource() override { stop(); }

    void setObserver(InventoryObserver *observer) override { m_observer = observer; }

    InventorySourceStartStatus start() override
    {
        if (m_running) {
            return InventorySourceStartStatus::Started;
        }
        if (!m_connection.isConnected() || m_connection.interface() == nullptr) {
            return InventorySourceStartStatus::InvalidConnection;
        }
        m_running = true;
        m_watcher = std::make_unique<QDBusServiceWatcher>(
            QString::fromLatin1(kCompositorService), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        QObject::connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged,
                         this, &QtCompositorInventorySource::serviceOwnerChanged);

        const QDBusReply<QString> owner =
            m_connection.interface()->serviceOwner(QString::fromLatin1(kCompositorService));
        if (!owner.isValid() || owner.value().isEmpty()) {
            notifyUnavailable();
            return InventorySourceStartStatus::Started;
        }
        if (!bindOwner(owner.value())) {
            stop();
            return InventorySourceStartStatus::SignalRegistrationFailed;
        }
        requestRead();
        return InventorySourceStartStatus::Started;
    }

    void stop() override
    {
        if (!m_running) {
            return;
        }
        m_running = false;
        ++m_requestSerial;
        if (!m_owner.isEmpty()) {
            (void)m_connection.disconnect(
                m_owner, QString::fromLatin1(kCompositorPath),
                QString::fromLatin1(kCompositorInterface),
                QStringLiteral("OutputsChanged"), this, SLOT(outputsChanged()));
        }
        m_owner.clear();
        m_inFlight = false;
        m_dirty = false;
        m_watcher.reset();
    }

private Q_SLOTS:
    void outputsChanged()
    {
        if (!m_running || m_owner.isEmpty()) {
            return;
        }
        m_dirty = true;
        if (!m_inFlight) {
            requestRead();
        }
    }

    void serviceOwnerChanged(const QString &, const QString &, const QString &newOwner)
    {
        if (!m_running || newOwner == m_owner) {
            return;
        }
        ++m_requestSerial;
        if (!m_owner.isEmpty()) {
            (void)m_connection.disconnect(
                m_owner, QString::fromLatin1(kCompositorPath),
                QString::fromLatin1(kCompositorInterface),
                QStringLiteral("OutputsChanged"), this, SLOT(outputsChanged()));
        }
        m_owner.clear();
        m_inFlight = false;
        m_dirty = false;
        notifyUnavailable();
        if (newOwner.isEmpty()) {
            return;
        }
        if (!bindOwner(newOwner)) {
            notifyUnavailable();
            return;
        }
        requestRead();
    }

private:
    bool bindOwner(const QString &owner)
    {
        if (!Private::validUniqueBusOwner(owner)) {
            return false;
        }
        if (!m_connection.connect(
                owner, QString::fromLatin1(kCompositorPath),
                QString::fromLatin1(kCompositorInterface),
                QStringLiteral("OutputsChanged"), this, SLOT(outputsChanged()))) {
            return false;
        }
        m_owner = owner;
        return true;
    }

    void requestRead()
    {
        if (!m_running || m_owner.isEmpty() || m_inFlight) {
            return;
        }
        m_inFlight = true;
        m_dirty = false;
        const QString requestedOwner = m_owner;
        const quint64 serial = ++m_requestSerial;
        QDBusMessage message = QDBusMessage::createMethodCall(
            requestedOwner, QString::fromLatin1(kCompositorPath),
            QString::fromLatin1(kCompositorInterface), QStringLiteral("Outputs"));
        auto *watcher = new QDBusPendingCallWatcher(
            m_connection.asyncCall(message, 5'000), this);
        QObject::connect(
            watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestedOwner, serial](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QByteArray> reply = *finished;
                finished->deleteLater();
                if (!m_running || serial != m_requestSerial
                    || requestedOwner != m_owner) {
                    return;
                }
                m_inFlight = false;
                if (reply.isError()) {
                    notifyUnavailable();
                } else {
                    const InventoryDecodeResult decoded =
                        decodeCompositorInventory(reply.value(), requestedOwner);
                    if (decoded.accepted()) {
                        if (m_observer != nullptr) {
                            m_observer->inventoryObserved(decoded.frame);
                        }
                    } else {
                        notifyUnavailable();
                    }
                }
                if (m_dirty) {
                    requestRead();
                }
            });
    }

    void notifyUnavailable()
    {
        if (m_observer != nullptr) {
            m_observer->inventoryUnavailable();
        }
    }

    QDBusConnection m_connection;
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    InventoryObserver *m_observer = nullptr;
    QString m_owner;
    quint64 m_requestSerial = 0;
    bool m_running = false;
    bool m_inFlight = false;
    bool m_dirty = false;
};

} // namespace

std::unique_ptr<InventorySource> makeCompositorInventorySource(
    const QDBusConnection &connection)
{
    return std::make_unique<QtCompositorInventorySource>(connection);
}

} // namespace QindaQt::DisplayService

#include "compositor_inventory_source.moc"
