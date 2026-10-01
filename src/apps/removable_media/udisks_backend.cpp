// SPDX-License-Identifier: GPL-3.0-or-later
#include "udisks_backend.h"
#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QStorageInfo>
#include <utility>

namespace QindaQt::Apps::RemovableMedia {
namespace {
const QString Service = QStringLiteral("org.freedesktop.UDisks2");
const QString Root = QStringLiteral("/org/freedesktop/UDisks2");
const QString ObjectManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");
}
UDisksBackend::UDisksBackend(QDBusConnection connection, QObject *parent)
    : MediaBackend(parent), m_bus(std::move(connection)),
      m_watcher(Service, m_bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    registerMediaDBusTypes();
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &UDisksBackend::refresh);
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) { ownerChanged(owner); });
    m_bus.connect(Service, Root, ObjectManager, QStringLiteral("InterfacesAdded"), this,
                  SLOT(interfacesAdded(QDBusObjectPath,Interfaces)));
    m_bus.connect(Service, Root, ObjectManager, QStringLiteral("InterfacesRemoved"), this,
                  SLOT(interfacesRemoved(QDBusObjectPath,QStringList)));
    m_bus.connect(Service, {}, QStringLiteral("org.freedesktop.DBus.Properties"),
                  QStringLiteral("PropertiesChanged"), this,
                  SLOT(propertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    QTimer::singleShot(0, this, [this] {
        const auto reply = m_bus.interface() ? m_bus.interface()->serviceOwner(Service) : QDBusReply<QString>{};
        if (reply.isValid()) ownerChanged(reply.value());
        else { m_diagnostic = QStringLiteral("The system disk service is unavailable. Install or start UDisks2."); Q_EMIT changed(); }
    });
}
const Volume *UDisksBackend::find(const QString &token) const
{
    for (const auto &v : m_volumes) if (v.token == token) return &v;
    return nullptr;
}
void UDisksBackend::ownerChanged(const QString &owner)
{
    ++m_epoch;
    ++m_inventorySerial;
    m_owner = owner;
    m_available = false;
    m_volumes.clear();
    m_objects.clear();
    m_formatTypes.clear();
    m_diagnostic = QStringLiteral("The system disk service is unavailable. Refresh to try again.");
    if (m_request) finish(false, QStringLiteral("The disk service changed during the operation. Check the media before trying again."));
    Q_EMIT changed();
    if (!m_owner.isEmpty()) { refresh(); discoverFormats(); }
}
void UDisksBackend::refresh()
{
    if (m_owner.isEmpty() && m_bus.interface()) {
        const auto reply = m_bus.interface()->serviceOwner(Service);
        if (reply.isValid() && !reply.value().isEmpty()) { ownerChanged(reply.value()); return; }
    }
    fetch();
}
void UDisksBackend::fetch(std::function<void(bool)> continuation)
{
    if (m_owner.isEmpty()) { if (continuation) continuation(false); return; }
    const quint64 serial = ++m_inventorySerial;
    const quint64 epoch = m_epoch;
    Step step{Root, ObjectManager, QStringLiteral("GetManagedObjects"), {}};
    call(step, [this, serial, epoch, continuation = std::move(continuation)](const QDBusMessage &reply) {
        if (epoch != m_epoch || serial != m_inventorySerial) {
            if (continuation) continuation(false);
            return;
        }
        const QDBusReply<ManagedObjects> objects(reply);
        if (!objects.isValid()) {
            m_available = false;
            m_volumes.clear();
            m_diagnostic = QStringLiteral("Could not read removable media: ") + objects.error().message();
            Q_EMIT changed();
            if (continuation) continuation(false);
            return;
        }
        publish(objects.value());
        if (continuation) continuation(true);
    });
}
void UDisksBackend::publish(const ManagedObjects &objects)
{
    auto next = projectVolumes(objects);
    for (auto &v : next) {
        for (const auto &old : std::as_const(m_volumes))
            if (v.identity == old.identity) { v.token = old.token; break; }
        if (v.token.isEmpty()) v.token = QString::number(m_epoch) + QLatin1Char('-') + QString::number(++m_attachment);
        if (!v.mountPath.isEmpty()) {
            QStorageInfo storage(v.mountPath);
            if (storage.isValid() && storage.isReady()) v.readOnly = v.readOnly || storage.isReadOnly();
        }
    }
    m_objects = objects;
    m_volumes = std::move(next);
    m_available = true;
    m_diagnostic.clear();
    Q_EMIT changed();
}
void UDisksBackend::call(const Step &step, ReplyHandler handler)
{
    const auto epoch = m_epoch;
    QDBusMessage message = QDBusMessage::createMethodCall(m_owner, step.path, step.interface, step.method);
    message.setArguments(step.arguments);
    message.setInteractiveAuthorizationAllowed(true);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 120'000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, epoch, handler = std::move(handler)](QDBusPendingCallWatcher *pending) {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (epoch == m_epoch) handler(reply);
    });
}
void UDisksBackend::discoverFormats()
{
    for (const QString &type : {QStringLiteral("exfat"), QStringLiteral("vfat"), QStringLiteral("ext4")}) {
        Step step{Root + QStringLiteral("/Manager"), QStringLiteral("org.freedesktop.UDisks2.Manager"),
                  QStringLiteral("CanFormat"), {type}};
        call(step, [this, type](const QDBusMessage &reply) {
            if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1) return;
            const auto argument = qvariant_cast<QDBusArgument>(reply.arguments().constFirst());
            bool supported = false;
            QString requiredProgram;
            argument.beginStructure();
            argument >> supported >> requiredProgram;
            argument.endStructure();
            if (supported && !m_formatTypes.contains(type)) { m_formatTypes.append(type); Q_EMIT changed(); }
        });
    }
}
void UDisksBackend::interfacesAdded(const QDBusObjectPath &, const Interfaces &) { m_debounce.start(); }
void UDisksBackend::interfacesRemoved(const QDBusObjectPath &path, const QStringList &)
{
    // Revoke before a later inventory call can observe a reused object path.
    for (qsizetype i = m_volumes.size(); i > 0; --i) {
        const auto &v = m_volumes.at(i - 1);
        if (v.path == path.path() || v.drive == path.path()) m_volumes.removeAt(i - 1);
    }
    ++m_inventorySerial;
    m_objects.remove(path);
    Q_EMIT changed();
    m_debounce.start();
}
void UDisksBackend::propertiesChanged(const QString &interface, const QVariantMap &properties,
                                     const QStringList &, const QDBusMessage &message)
{
    if (message.service() != m_owner || !interface.startsWith(Service)) return;
    if (properties.contains(QStringLiteral("MediaAvailable"))
        && !properties.value(QStringLiteral("MediaAvailable")).toBool()) {
        interfacesRemoved(QDBusObjectPath(message.path()), {});
    } else m_debounce.start();
}
} // namespace QindaQt::Apps::RemovableMedia
