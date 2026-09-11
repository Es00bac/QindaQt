// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_device_port.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QHash>
#include <QVariantList>

namespace QindaQt::Apps::SettingsInput {
namespace {

constexpr auto KWinService = "org.kde.KWin";
constexpr auto ManagerPath = "/org/kde/KWin/InputDevice";
constexpr auto ManagerInterface = "org.kde.KWin.InputDeviceManager";
constexpr auto DeviceInterface = "org.kde.KWin.InputDevice";
constexpr auto PropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr int CallTimeoutMs = 4000;
constexpr int MaxDevices = 256;

// AGENT-GUARD: The writable-property table is closed and typed. A name
// outside it must never reach D-Bus: KWin would accept unknown booleans
// silently and the route would present unobserved truth as a change.
struct PropertyKind {
    enum class Type { Unknown, Boolean, Double };
    Type type;
};
PropertyKind propertyKind(const QString &property) {
    static const QHash<QString, PropertyKind> table{
        {QStringLiteral("enabled"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("leftHanded"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("naturalScroll"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("pointerAccelerationProfileFlat"),
         {PropertyKind::Type::Boolean}},
        {QStringLiteral("pointerAccelerationProfileAdaptive"),
         {PropertyKind::Type::Boolean}},
        {QStringLiteral("tapToClick"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("tapAndDrag"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("tapDragLock"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("disableWhileTyping"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("middleEmulation"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("lmrTapButtonMap"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("scrollTwoFinger"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("scrollEdge"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("scrollOnButtonDown"), {PropertyKind::Type::Boolean}},
        {QStringLiteral("pointerAcceleration"), {PropertyKind::Type::Double}},
        {QStringLiteral("scrollFactor"), {PropertyKind::Type::Double}},
    };
    return table.value(property, {PropertyKind::Type::Unknown});
}

QDBusMessage call(const QDBusConnection &bus, const QString &service,
                  const QString &path, const QString &interface,
                  const QString &method, const QVariantList &arguments = {}) {
    QDBusMessage message =
        QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(arguments);
    return bus.call(message, QDBus::Block, CallTimeoutMs);
}

bool serviceAvailable(const QDBusConnection &bus) {
    return bus.isConnected() && bus.interface() != nullptr &&
           bus.interface()->isServiceRegistered(QLatin1String(KWinService));
}

} // namespace

PointerDevicePort::~PointerDevicePort() = default;

KWinPointerDevicePort::KWinPointerDevicePort(QDBusConnection bus)
    : m_bus(std::move(bus)) {}

QList<PointerDeviceSnapshot>
KWinPointerDevicePort::devices(QString *error) const {
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Input authority org.kde.KWin is not reachable");
        }
        return {};
    }
    const QDBusMessage reply = call(m_bus, QLatin1String(KWinService),
                                    QLatin1String(ManagerPath),
                                    QLatin1String(ManagerInterface),
                                    QStringLiteral("ListPointers"));
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("ListPointers failed: %1")
                         .arg(reply.errorMessage());
        }
        return {};
    }
    if (reply.arguments().size() != 1 ||
        reply.arguments().at(0).userType() != QMetaType::QStringList) {
        if (error != nullptr) {
            *error = QStringLiteral("ListPointers reply is malformed");
        }
        return {};
    }
    const QStringList sysNames = reply.arguments().at(0).toStringList();
    QList<PointerDeviceSnapshot> snapshots;
    for (const QString &sysName : sysNames) {
        // AGENT-GUARD: The authority is process-wide; a compromised or
        // future reply must not run this route into unbounded object reads.
        if (sysName.isEmpty() || sysName.contains(QLatin1Char('/')) ||
            snapshots.size() >= MaxDevices) {
            continue;
        }
        const QString path =
            QStringLiteral("/org/kde/KWin/InputDevice/%1").arg(sysName);
        const QDBusMessage deviceReply =
            call(m_bus, QLatin1String(KWinService), path,
                 QLatin1String(PropertiesInterface), QStringLiteral("GetAll"),
                 {QLatin1String(DeviceInterface)});
        // AGENT-NOTE: The reply container may arrive as a demarshalled
        // QVariantMap or as a lazy QDBusArgument; both are accepted below,
        // anything else is treated as a vanished device.
        const bool replyShapeOk =
            deviceReply.type() == QDBusMessage::ReplyMessage &&
            deviceReply.arguments().size() == 1 &&
            (deviceReply.arguments().at(0).typeId() ==
                 QMetaType::QVariantMap ||
             deviceReply.arguments().at(0).canConvert<QDBusArgument>());
        if (!replyShapeOk) {
            // A device that vanished mid-listing is skipped, not fatal;
            // anything else here would already have been reported above.
            continue;
        }
        // AGENT-NOTE: Qt may hand the reply container back as a lazy
        // QDBusArgument instead of a demarshalled QVariantMap; accept both
        // forms or every device would be silently dropped.
        QVariantMap raw;
        const QVariant rawArgument = deviceReply.arguments().at(0);
        if (rawArgument.typeId() == QMetaType::QVariantMap) {
            raw = rawArgument.toMap();
        } else if (rawArgument.canConvert<QDBusArgument>()) {
            const QDBusArgument argument = rawArgument.value<QDBusArgument>();
            argument.beginMap();
            while (!argument.atEnd()) {
                argument.beginMapEntry();
                QString key;
                QVariant value;
                argument >> key >> value;
                argument.endMapEntry();
                raw.insert(key, value);
            }
            argument.endMap();
        }
        PointerDeviceSnapshot snapshot;
        snapshot.deviceId = sysName;
        snapshot.name = raw.value(QStringLiteral("name")).toString();
        snapshot.touchpad =
            raw.value(QStringLiteral("touchpad")).typeId() ==
                    QMetaType::Bool &&
                raw.value(QStringLiteral("touchpad")).toBool();
        snapshot.pointer =
            raw.value(QStringLiteral("pointer")).typeId() == QMetaType::Bool &&
            raw.value(QStringLiteral("pointer")).toBool();
        // AGENT-GUARD: A device that reports neither kind means the
        // authority's reply cannot be interpreted; presenting it as a row
        // would be fabricated truth. Fail the whole listing closed.
        if (!snapshot.pointer && !snapshot.touchpad) {
            if (error != nullptr) {
                *error = QStringLiteral("Device %1 did not report a usable "
                                        "kind; the reply is uninterpretable")
                             .arg(sysName);
            }
            return {};
        }
        for (const QString &property : {
                 QStringLiteral("enabled"),
                 QStringLiteral("leftHanded"),
                 QStringLiteral("naturalScroll"),
                 QStringLiteral("pointerAcceleration"),
                 QStringLiteral("pointerAccelerationProfileFlat"),
                 QStringLiteral("pointerAccelerationProfileAdaptive"),
                 QStringLiteral("scrollFactor"),
                 QStringLiteral("tapToClick"),
                 QStringLiteral("tapAndDrag"),
                 QStringLiteral("tapDragLock"),
                 QStringLiteral("disableWhileTyping"),
                 QStringLiteral("middleEmulation"),
                 QStringLiteral("lmrTapButtonMap"),
                 QStringLiteral("scrollTwoFinger"),
                 QStringLiteral("scrollEdge"),
                 QStringLiteral("scrollOnButtonDown"),
                 QStringLiteral("supportsLeftHanded"),
                 QStringLiteral("supportsNaturalScroll"),
                 QStringLiteral("supportsPointerAcceleration"),
                 QStringLiteral("supportsPointerAccelerationProfileFlat"),
                 QStringLiteral("supportsPointerAccelerationProfileAdaptive"),
                 QStringLiteral("supportsTapToClick" ),
                 QStringLiteral("supportsTapAndDrag"),
                 QStringLiteral("supportsDisableWhileTyping"),
                 QStringLiteral("supportsMiddleEmulation"),
                 QStringLiteral("supportsLmrTapButtonMap"),
                 QStringLiteral("supportsScrollTwoFinger"),
                 QStringLiteral("supportsScrollEdge"),
                 QStringLiteral("supportsScrollOnButtonDown"),
                 QStringLiteral("defaultLeftHanded"),
                 QStringLiteral("defaultNaturalScroll"),
                 QStringLiteral("defaultPointerAcceleration"),
                 QStringLiteral("defaultPointerAccelerationProfileFlat"),
                 QStringLiteral("defaultPointerAccelerationProfileAdaptive"),
                 QStringLiteral("defaultScrollFactor"),
                 QStringLiteral("defaultTapToClick"),
                 QStringLiteral("defaultTapAndDrag"),
                 QStringLiteral("defaultDisableWhileTyping"),
                 QStringLiteral("defaultMiddleEmulation"),
                 QStringLiteral("defaultScrollTwoFinger"),
                 QStringLiteral("defaultScrollEdge"),
             }) {
            if (raw.contains(property)) {
                snapshot.properties.insert(property, raw.value(property));
            }
        }
        snapshots.append(snapshot);
    }
    return snapshots;
}

bool KWinPointerDevicePort::writeProperty(const QString &deviceId,
                                          const QString &property,
                                          const QVariant &value,
                                          QString *error) const {
    const PropertyKind kind = propertyKind(property);
    if (kind.type == PropertyKind::Type::Unknown) {
        if (error != nullptr) {
            *error = QStringLiteral("Unknown pointer property '%1'")
                         .arg(property);
        }
        return false;
    }
    const bool typeOk =
        kind.type == PropertyKind::Type::Boolean
            ? value.typeId() == QMetaType::Bool
            : value.typeId() == QMetaType::Double;
    if (!typeOk) {
        if (error != nullptr) {
            *error = QStringLiteral("Refusing to write property '%1' with a "
                                    "value of the wrong type")
                         .arg(property);
        }
        return false;
    }
    if (deviceId.isEmpty() || deviceId.contains(QLatin1Char('/')) ||
        !serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral("Input authority org.kde.KWin is not "
                                    "reachable for device '%1'")
                         .arg(deviceId);
        }
        return false;
    }
    const QString path =
        QStringLiteral("/org/kde/KWin/InputDevice/%1").arg(deviceId);
    const QDBusMessage reply =
        call(m_bus, QLatin1String(KWinService), path,
             QLatin1String(PropertiesInterface), QStringLiteral("Set"),
             {QLatin1String(DeviceInterface), property,
              QVariant::fromValue(QDBusVariant(value))});
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("Setting %1 on %2 failed: %3")
                         .arg(property, deviceId, reply.errorMessage());
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Apps::SettingsInput
