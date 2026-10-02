// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/session/display_power/scoped_display_power.h>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>
#include <cmath>
#include <unistd.h>
#include <utility>
namespace QindaQt::Session::DisplayPower {
namespace {
const QString Path = QStringLiteral("/org/qindaqt/KWin/DisplayPower");
const QString Interface = QStringLiteral("org.qindaqt.KWin.DisplayPower1");
bool validId(const QString &id) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}$"));
    return pattern.match(id).hasMatch();
}
QDBusMessage call(const QString &owner, const QString &method, const QList<QVariant> &args) {
    auto message = QDBusMessage::createMethodCall(owner, Path, Interface, method);
    message.setArguments(args); return message;
}
bool inventory(const QByteArray &bytes, bool *off) {
    if (bytes.size() > 65536) return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) return false;
    const auto array = document.array();
    if (array.isEmpty() || array.size() > 64) return false;
    QSet<QString> ids;
    bool allOff = true;
    for (const auto &item : array) {
        if (!item.isObject()) return false;
        const auto record = item.toObject();
        if (record.size() != 5 || !record.value(QStringLiteral("id")).isString()
            || !record.value(QStringLiteral("name")).isString()
            || !record.value(QStringLiteral("dpms")).isBool()
            || !record.value(QStringLiteral("dpms")).toBool()
            || !record.value(QStringLiteral("on")).isBool()
            || !record.value(QStringLiteral("geometry")).isArray()) return false;
        const auto id = record.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || ids.contains(id)) return false;
        ids.insert(id);
        const auto geometry = record.value(QStringLiteral("geometry")).toArray();
        if (geometry.size() != 4) return false;
        for (qsizetype i = 0; i < 4; ++i) {
            if (!geometry[i].isDouble() || !std::isfinite(geometry[i].toDouble())
                || std::abs(geometry[i].toDouble()) > 1000000
                || (i >= 2 && geometry[i].toDouble() <= 0)) return false;
        }
        allOff = allOff && !record.value(QStringLiteral("on")).toBool();
    }
    *off = allOff; return true;
}
}
ScopedDisplayPower::ScopedDisplayPower(QDBusConnection bus, QString peerOwner, quint32 peerPid,
    std::function<bool()> lineageLive, bool nativeExclusive, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_owner(std::move(peerOwner)), m_pid(peerPid),
      m_lineage(std::move(lineageLive)), m_ownerWatch(QString(CompositorNames::service), m_bus,
        QDBusServiceWatcher::WatchForOwnerChange), m_native(nativeExclusive) {
    m_deadline.setSingleShot(true); m_deadline.setInterval(2000);
    connect(&m_deadline, &QTimer::timeout, this, [this] { stop(); });
    connect(&m_ownerWatch, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
        if (!live()) stop();
    });
    m_renew.setInterval(10000);
    connect(&m_renew, &QTimer::timeout, this, [this] {
        if (!live()) { stop(); return; }
        for (const auto &id : std::as_const(m_causes)) sendAcquire(id);
    });
}
ScopedDisplayPower::~ScopedDisplayPower() { stop(); }
bool ScopedDisplayPower::live() const {
    if (!m_lineage || !m_lineage() || !m_bus.isConnected() || !m_bus.interface()) return false;
    const auto current = m_bus.interface()->serviceOwner(QString(CompositorNames::service));
    const auto pid = m_bus.interface()->servicePid(m_owner);
    const auto uid = m_bus.interface()->serviceUid(m_owner);
    const auto session = m_bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    return current.isValid() && current.value() == m_owner && pid.isValid() && pid.value() == m_pid
        && uid.isValid() && uid.value() == static_cast<uint>(::getuid())
        && session.isValid() && session.value() == m_bus.baseService();
}
bool ScopedDisplayPower::start() {
    if (m_started) return true;
    if (!m_native || !live()) return false;
    m_started = true;
    if (!m_bus.connect(m_owner, Path, Interface, QStringLiteral("InventoryReceipt"), this, SLOT(inventoryReceipt(QDBusMessage)))
        || !m_bus.connect(m_owner, Path, Interface, QStringLiteral("InventoryChanged"), this, SLOT(inventoryChanged(QDBusMessage)))
        || !m_bus.connect(m_owner, Path, Interface, QStringLiteral("LeaseEnded"), this, SLOT(leaseEnded(QDBusMessage)))) {
        stop(); return false;
    }
    refresh(); return true;
}
void ScopedDisplayPower::setAvailable(bool available) {
    if (m_available == available) return;
    m_available = available; Q_EMIT availabilityChanged(available);
}
void ScopedDisplayPower::stop(bool waitForAcknowledgement) {
    if (!m_started) return;
    m_started = false; ++m_serial; m_deadline.stop(); m_renew.stop(); m_nonce.clear();
    setAvailable(false);
    // Retain the original constructing bus/unique peer even after name loss.
    // A bounded final acknowledgement never opens a replacement Wayland peer.
    if (m_epoch != 0 && m_bus.isConnected()) {
        const auto withdraw = call(m_owner, QStringLiteral("SetNativeAdmission"), {QVariant::fromValue(m_epoch), false});
        if (waitForAcknowledgement) static_cast<void>(m_bus.call(withdraw, QDBus::Block, 250));
        else static_cast<void>(m_bus.asyncCall(withdraw, 250));
    }
    const auto ids = m_causes; m_causes.clear();
    for (const auto &id : ids) Q_EMIT causeEnded(id, QStringLiteral("stopped"));
    m_bus.disconnect(m_owner, Path, Interface, QStringLiteral("InventoryReceipt"), this, SLOT(inventoryReceipt(QDBusMessage)));
    m_bus.disconnect(m_owner, Path, Interface, QStringLiteral("InventoryChanged"), this, SLOT(inventoryChanged(QDBusMessage)));
    m_bus.disconnect(m_owner, Path, Interface, QStringLiteral("LeaseEnded"), this, SLOT(leaseEnded(QDBusMessage)));
    m_epoch = 0;
}
bool ScopedDisplayPower::available() const { return m_available && m_started && live(); }
bool ScopedDisplayPower::off() const { return m_off; }
void ScopedDisplayPower::refresh() {
    if (!m_started || !m_nonce.isEmpty()) return;
    if (!live()) { stop(); return; }
    m_nonce = QUuid::createUuid().toString(QUuid::Id128);
    m_deadline.start();
    static_cast<void>(m_bus.asyncCall(call(m_owner, QStringLiteral("RequestInventoryWithReceipt"), {m_nonce}), 2000));
}
void ScopedDisplayPower::inventoryChanged(const QDBusMessage &message) {
    if (message.service() == m_owner && message.signature().isEmpty()) refresh();
}
void ScopedDisplayPower::inventoryReceipt(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (!m_started || !live() || message.service() != m_owner || message.signature() != QStringLiteral("stbbay")
        || args.size() != 5 || m_nonce.isEmpty() || args[0].toString() != m_nonce) return;
    m_nonce.clear(); m_deadline.stop();
    const quint64 epoch = args[1].toULongLong();
    bool off = false;
    if (epoch == 0 || !args[2].toBool() || !inventory(args[4].toByteArray(), &off)
        || (m_epoch != 0 && m_epoch != epoch)) { stop(); return; }
    m_epoch = epoch;
    if (m_off != off) { m_off = off; Q_EMIT powerChanged(off); }
    if (!args[3].toBool()) setAvailable(false);
    if (m_available) return;
    const auto serial = m_serial;
    auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(call(m_owner,
        QStringLiteral("SetNativeAdmission"), {QVariant::fromValue(epoch), true}), 2000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, pending, serial, epoch] {
        const QDBusPendingReply<bool> reply = *pending; pending->deleteLater();
        if (!m_started || serial != m_serial || m_epoch != epoch) return;
        if (!live() || reply.isError() || !reply.value()) { stop(); return; }
        setAvailable(true); m_renew.start();
    });
}
bool ScopedDisplayPower::acquire(const QString &id) {
    if (!validId(id) || !available() || m_causes.size() >= 64) return false;
    if (m_causes.contains(id)) return true;
    m_causes.insert(id); sendAcquire(id); return true;
}
void ScopedDisplayPower::sendAcquire(const QString &id) {
    QElapsedTimer clock; clock.start();
    const quint64 deadline = static_cast<quint64>(clock.msecsSinceReference() + 30000);
    const auto serial = m_serial, epoch = m_epoch;
    auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(call(m_owner, QStringLiteral("AcquireBlank"),
        {QVariant::fromValue(epoch), id, QVariant::fromValue(deadline)}), 2000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, pending, serial, epoch, id] {
        const QDBusPendingReply<bool> reply = *pending; pending->deleteLater();
        if (!m_started || serial != m_serial || epoch != m_epoch || !m_causes.contains(id)) return;
        const bool admitted = live() && !reply.isError() && reply.value();
        if (!admitted) stop();
        Q_EMIT acquisitionFinished(id, admitted);
    });
}
void ScopedDisplayPower::release(const QString &id) {
    if (!validId(id) || m_epoch == 0) return;
    m_causes.remove(id);
    static_cast<void>(m_bus.asyncCall(call(m_owner, QStringLiteral("ReleaseBlank"), {QVariant::fromValue(m_epoch), id}), 2000));
}
void ScopedDisplayPower::leaseEnded(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (!m_started || message.service() != m_owner || message.signature() != QStringLiteral("ss") || args.size() != 2 || !live()) return;
    const auto id = args[0].toString();
    if (m_causes.remove(id) == 0) return;
    Q_EMIT causeEnded(id, args[1].toString());
    refresh();
}
}
