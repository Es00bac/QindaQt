// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluez_pairing_agent.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtDBus/QDBusPendingCallWatcher>

namespace QindaQt::Bluetooth::Bluez
{
namespace
{
constexpr QLatin1StringView kAgentPath{"/org/qindaqt/BluetoothAgent"};
constexpr QLatin1StringView kAgentManager{"org.bluez.AgentManager1"};
constexpr QLatin1StringView kRejected{"org.bluez.Error.Rejected"};
constexpr QLatin1StringView kCanceled{"org.bluez.Error.Canceled"};

QString passkeyText(const quint32 passkey)
{
    return passkey <= 999999 ? QString::number(passkey).rightJustified(6, QLatin1Char('0'))
                             : QString{};
}
} // namespace

BluezPairingAgent::BluezPairingAgent(const QDBusConnection &connection,
                                     DeviceResolver resolver,
                                     const int timeoutMs, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
    , m_resolver(std::move(resolver))
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(qBound(10, timeoutMs, kPairingPromptTimeoutMs));
    connect(&m_timer, &QTimer::timeout, this, [this] {
        rejectCurrent(QString(kCanceled));
    });
}

BluezPairingAgent::~BluezPairingAgent()
{
    stop();
}

void BluezPairingAgent::adoptOwner(const QString &owner)
{
    if (owner == m_owner && m_objectRegistered) {
        return;
    }
    rejectCurrent(QString(kCanceled));
    m_owner = owner;
    ++m_ownerToken;
    if (!m_objectRegistered && m_connection.isConnected()) {
        m_objectRegistered = m_connection.registerObject(
            QString(kAgentPath), this, QDBusConnection::ExportScriptableSlots);
    }
    if (!m_owner.isEmpty() && m_objectRegistered) {
        registerWithOwner();
    }
}

void BluezPairingAgent::stop()
{
    rejectCurrent(QString(kCanceled));
    ++m_ownerToken;
    m_owner.clear();
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString(kAgentPath));
        m_objectRegistered = false;
    }
}

void BluezPairingAgent::registerWithOwner()
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        m_owner, QStringLiteral("/org/bluez"), QString(kAgentManager),
        QStringLiteral("RegisterAgent"));
    call.setArguments({QVariant::fromValue(QDBusObjectPath(QString(kAgentPath))),
                       QStringLiteral("KeyboardDisplay")});
    const quint64 token = m_ownerToken;
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                if (token != m_ownerToken || m_owner.isEmpty()) {
                    return;
                }
                // Registration failure remains fail-closed: Device1.Pair will
                // report a typed BlueZ failure and no prompt can be accepted.
            });
}

bool BluezPairingAgent::authenticCall() const
{
    return calledFromDBus() && !m_owner.isEmpty() && message().service() == m_owner;
}

bool BluezPairingAgent::requireAuthenticCall()
{
    if (authenticCall()) {
        return true;
    }
    if (calledFromDBus()) {
        sendErrorReply(QString(kRejected), QStringLiteral("Pairing caller rejected"));
    }
    return false;
}

QString BluezPairingAgent::resolve(const QString &devicePath) const
{
    return m_resolver ? m_resolver(devicePath) : QString{};
}

void BluezPairingAgent::beginRequest(const PairingPromptKind kind,
                                     const QString &devicePath, QString detail,
                                     QString serviceUuid)
{
    if (!requireAuthenticCall()) {
        return;
    }
    const QDBusMessage call = message();
    setDelayedReply(true);
    const QString address = resolve(devicePath);
    if (address.isEmpty() || m_prompt.kind != PairingPromptKind::None
        || !isBoundedText(detail, kMaxPairingTextUtf8Bytes)
        || !isBoundedText(serviceUuid, kMaxPairingTextUtf8Bytes)) {
        m_connection.send(call.createErrorReply(QString(kRejected),
                                                QStringLiteral("Pairing prompt rejected")));
        return;
    }
    m_pendingCall = call;
    m_prompt = {.kind = kind,
                .deviceAddress = address,
                .detail = std::move(detail),
                .serviceUuid = std::move(serviceUuid),
                .entered = 0};
    m_timer.start();
    Q_EMIT promptChanged(m_prompt);
}

void BluezPairingAgent::publishDisplay(const PairingPromptKind kind,
                                       const QString &devicePath, QString detail,
                                       const quint16 entered)
{
    if (!requireAuthenticCall()) {
        return;
    }
    const QString address = resolve(devicePath);
    if (address.isEmpty() || !isBoundedText(detail, kMaxPairingTextUtf8Bytes)
        || (m_prompt.kind != PairingPromptKind::None
            && (m_prompt.kind != kind || m_prompt.deviceAddress != address))) {
        sendErrorReply(QString(kRejected), QStringLiteral("Pairing display rejected"));
        return;
    }
    m_prompt = {.kind = kind,
                .deviceAddress = address,
                .detail = std::move(detail),
                .serviceUuid = {},
                .entered = entered};
    m_timer.start();
    Q_EMIT promptChanged(m_prompt);
}

void BluezPairingAgent::clearPrompt()
{
    m_timer.stop();
    m_pendingCall = QDBusMessage{};
    if (m_prompt.kind == PairingPromptKind::None) {
        return;
    }
    m_prompt = {};
    Q_EMIT promptChanged(m_prompt);
}

void BluezPairingAgent::rejectCurrent(const QString &errorName)
{
    const QDBusMessage pending = m_pendingCall;
    if (pending.type() == QDBusMessage::MethodCallMessage) {
        m_connection.send(pending.createErrorReply(
            errorName, QStringLiteral("Pairing prompt canceled")));
    }
    clearPrompt();
}

bool BluezPairingAgent::replyConfirmation(const bool accepted)
{
    if (m_prompt.kind != PairingPromptKind::ConfirmPasskey
        && m_prompt.kind != PairingPromptKind::AuthorizeService) {
        return false;
    }
    const QDBusMessage pending = m_pendingCall;
    if (pending.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    accepted ? m_connection.send(pending.createReply())
             : m_connection.send(pending.createErrorReply(
                   QString(kRejected), QStringLiteral("Pairing rejected")));
    clearPrompt();
    return true;
}

bool BluezPairingAgent::replyPasskey(const QString &passkey)
{
    bool ok = false;
    const quint32 value = passkey.toUInt(&ok);
    if (m_prompt.kind != PairingPromptKind::EnterPasskey || !ok
        || passkey.isEmpty() || passkey.size() > 6 || value > 999999) {
        return false;
    }
    const QDBusMessage pending = m_pendingCall;
    if (pending.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    m_connection.send(pending.createReply(QVariant::fromValue(value)));
    clearPrompt();
    return true;
}

bool BluezPairingAgent::replyPin(const QString &pin)
{
    if (m_prompt.kind != PairingPromptKind::EnterPin || pin.isEmpty()
        || pin.size() > 16 || !isBoundedText(pin, kMaxPairingTextUtf8Bytes)) {
        return false;
    }
    const QDBusMessage pending = m_pendingCall;
    if (pending.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    m_connection.send(pending.createReply(QVariant::fromValue(pin)));
    clearPrompt();
    return true;
}

bool BluezPairingAgent::cancelPrompt()
{
    if (m_prompt.kind == PairingPromptKind::None) {
        return false;
    }
    rejectCurrent(QString(kCanceled));
    return true;
}

void BluezPairingAgent::Release()
{
    if (!requireAuthenticCall()) {
        return;
    }
    rejectCurrent(QString(kCanceled));
}

void BluezPairingAgent::RequestPinCode(const QDBusObjectPath &device)
{
    beginRequest(PairingPromptKind::EnterPin, device.path());
}

void BluezPairingAgent::DisplayPinCode(const QDBusObjectPath &device,
                                       const QString &pinCode)
{
    publishDisplay(PairingPromptKind::DisplayPin, device.path(), pinCode);
}

void BluezPairingAgent::RequestPasskey(const QDBusObjectPath &device)
{
    beginRequest(PairingPromptKind::EnterPasskey, device.path());
}

void BluezPairingAgent::DisplayPasskey(const QDBusObjectPath &device,
                                       const quint32 passkey,
                                       const ushort entered)
{
    publishDisplay(PairingPromptKind::DisplayPasskey, device.path(),
                   passkeyText(passkey), entered);
}

void BluezPairingAgent::RequestConfirmation(const QDBusObjectPath &device,
                                            const quint32 passkey)
{
    beginRequest(PairingPromptKind::ConfirmPasskey, device.path(),
                 passkeyText(passkey));
}

void BluezPairingAgent::RequestAuthorization(const QDBusObjectPath &device)
{
    Q_UNUSED(device)
    if (!requireAuthenticCall()) {
        return;
    }
    sendErrorReply(QString(kRejected),
                   QStringLiteral("Incoming authorization is unsupported"));
}

void BluezPairingAgent::AuthorizeService(const QDBusObjectPath &device,
                                         const QString &uuid)
{
    beginRequest(PairingPromptKind::AuthorizeService, device.path(), {}, uuid);
}

void BluezPairingAgent::Cancel()
{
    if (!requireAuthenticCall()) {
        return;
    }
    rejectCurrent(QString(kCanceled));
}

} // namespace QindaQt::Bluetooth::Bluez
