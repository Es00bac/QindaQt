// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/kwin_tablet_devices.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QRectF>
#include <QHash>
#include <QVariantList>

namespace QindaQt::Services::TabletDevices {
namespace {

constexpr auto KWinService = "org.kde.KWin";
constexpr auto ManagerPath = "/org/kde/KWin/InputDevice";
constexpr auto ManagerInterface = "org.kde.KWin.InputDeviceManager";
constexpr auto DeviceInterface = "org.kde.KWin.InputDevice";
constexpr auto PropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr int CallTimeoutMs = 4000;
constexpr int MaxDevices = 256;

enum class ValueKind { Unknown, Boolean, Double, UnsignedInt, Text, Area };

ValueKind writableKind(const QString &property) {
    static const QHash<QString, ValueKind> table{
        {QStringLiteral("enabled"), ValueKind::Boolean},
        {QStringLiteral("leftHanded"), ValueKind::Boolean},
        {QStringLiteral("mapToWorkspace"), ValueKind::Boolean},
        {QStringLiteral("tabletToolIsRelative"), ValueKind::Boolean},
        {QStringLiteral("pressureRangeMin"), ValueKind::Double},
        {QStringLiteral("pressureRangeMax"), ValueKind::Double},
        {QStringLiteral("rotation"), ValueKind::UnsignedInt},
        {QStringLiteral("outputName"), ValueKind::Text},
        {QStringLiteral("calibrationMatrix"), ValueKind::Text},
        {QStringLiteral("pressureCurve"), ValueKind::Text},
        {QStringLiteral("outputArea"), ValueKind::Area},
        {QStringLiteral("inputArea"), ValueKind::Area},
    };
    return table.value(property, ValueKind::Unknown);
}

// The closed set of device properties this feature reads. Anything else KWin
// publishes is deliberately dropped so a future KWin key cannot silently
// widen what the route presents.
const QStringList &readProperties() {
    static const QStringList properties{
        QStringLiteral("enabled"),
        QStringLiteral("outputName"),
        QStringLiteral("mapToWorkspace"),
        QStringLiteral("outputArea"),
        QStringLiteral("inputArea"),
        QStringLiteral("calibrationMatrix"),
        QStringLiteral("pressureCurve"),
        QStringLiteral("pressureRangeMin"),
        QStringLiteral("pressureRangeMax"),
        QStringLiteral("rotation"),
        QStringLiteral("leftHanded"),
        QStringLiteral("tabletToolIsRelative"),
        QStringLiteral("size"),
        QStringLiteral("tabletPadButtonCount"),
        QStringLiteral("tabletPadRingCount"),
        QStringLiteral("tabletPadStripCount"),
        QStringLiteral("tabletPadDialCount"),
        QStringLiteral("supportsDisableEvents"),
        QStringLiteral("supportsOutputArea"),
        QStringLiteral("supportsInputArea"),
        QStringLiteral("supportsCalibrationMatrix"),
        QStringLiteral("supportsPressureRange"),
        QStringLiteral("supportsRotation"),
        QStringLiteral("supportsLeftHanded"),
        QStringLiteral("defaultOutputArea"),
        QStringLiteral("defaultInputArea"),
        QStringLiteral("defaultCalibrationMatrix"),
        QStringLiteral("defaultPressureCurve"),
        QStringLiteral("defaultPressureRangeMin"),
        QStringLiteral("defaultPressureRangeMax"),
        QStringLiteral("defaultRotation"),
        QStringLiteral("defaultMapToWorkspace"),
    };
    return properties;
}

QDBusMessage call(const QDBusConnection &bus, const QString &path,
                  const QString &interface, const QString &method,
                  const QVariantList &arguments = {}) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(KWinService), path, interface, method);
    message.setArguments(arguments);
    return bus.call(message, QDBus::Block, CallTimeoutMs);
}

bool serviceAvailable(const QDBusConnection &bus) {
    return bus.isConnected() && bus.interface() != nullptr &&
           bus.interface()->isServiceRegistered(QLatin1String(KWinService));
}

bool legalSysName(const QString &sysName) {
    return !sysName.isEmpty() && !sysName.contains(QLatin1Char('/')) &&
           !sysName.contains(QLatin1Char('.'));
}

QString devicePath(const QString &sysName) {
    return QStringLiteral("/org/kde/KWin/InputDevice/%1").arg(sysName);
}

// AGENT-NOTE: Qt hands a reply container back either demarshalled or as a
// lazy QDBusArgument; accept both or every device is silently dropped.
QVariantMap demarshalMap(const QVariant &raw, bool *ok) {
    if (raw.typeId() == QMetaType::QVariantMap) {
        *ok = true;
        return raw.toMap();
    }
    if (!raw.canConvert<QDBusArgument>()) {
        *ok = false;
        return {};
    }
    const QDBusArgument argument = raw.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::MapType) {
        *ok = false;
        return {};
    }
    QVariantMap map;
    argument.beginMap();
    while (!argument.atEnd()) {
        argument.beginMapEntry();
        QString key;
        QVariant value;
        argument >> key >> value;
        argument.endMapEntry();
        map.insert(key, value);
    }
    argument.endMap();
    *ok = true;
    return map;
}

// KWin's `(dd)` and `(dddd)` structs arrive as lazy QDBusArguments; flatten
// them into a plain list of doubles so no transport type reaches a model.
QVariant flattenStruct(const QVariant &raw) {
    if (!raw.canConvert<QDBusArgument>()) {
        return raw;
    }
    const QDBusArgument argument = raw.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::StructureType) {
        return {};
    }
    // AGENT-GUARD: Read the members as doubles, not as QVariant. A struct
    // member is a bare `d` on the wire, and QDBusArgument's QVariant reader
    // expects a `v` there — asking for one crashes inside libdbus rather
    // than failing. Every struct this port reads is all-doubles ((dd) size,
    // (dddd) areas), so the element type is known.
    QVariantList members;
    argument.beginStructure();
    while (!argument.atEnd()) {
        if (argument.currentType() != QDBusArgument::BasicType) {
            argument.endStructure();
            return {};
        }
        double member = 0.0;
        argument >> member;
        members.append(member);
    }
    argument.endStructure();
    return members;
}

bool isStructProperty(const QString &property) {
    return property == QLatin1String("size") ||
           property == QLatin1String("outputArea") ||
           property == QLatin1String("inputArea") ||
           property == QLatin1String("defaultOutputArea") ||
           property == QLatin1String("defaultInputArea");
}

bool snapshotFromReply(const QString &sysName, const QVariantMap &raw,
                       TabletDeviceSnapshot *snapshot) {
    const QVariant tool = raw.value(QStringLiteral("tabletTool"));
    const QVariant pad = raw.value(QStringLiteral("tabletPad"));
    if (tool.typeId() != QMetaType::Bool || pad.typeId() != QMetaType::Bool) {
        return false;
    }
    snapshot->deviceId = sysName;
    snapshot->name = raw.value(QStringLiteral("name")).toString();
    snapshot->deviceGroupId =
        raw.value(QStringLiteral("deviceGroupId")).toString();
    snapshot->vendorId = raw.value(QStringLiteral("vendor")).toUInt();
    snapshot->productId = raw.value(QStringLiteral("product")).toUInt();
    snapshot->tabletTool = tool.toBool();
    snapshot->tabletPad = pad.toBool();
    snapshot->properties.clear();
    const QStringList &wanted = readProperties();
    for (const QString &property : wanted) {
        if (!raw.contains(property)) {
            continue;
        }
        const QVariant value = isStructProperty(property)
                                   ? flattenStruct(raw.value(property))
                                   : raw.value(property);
        if (value.isValid()) {
            snapshot->properties.insert(property, value);
        }
    }
    return true;
}

} // namespace

KWinTabletDevicePort::KWinTabletDevicePort(QDBusConnection bus)
    : m_bus(std::move(bus)) {}

QList<TabletDeviceSnapshot>
KWinTabletDevicePort::devices(QString *error) const {
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Input authority org.kde.KWin is not reachable");
        }
        return {};
    }
    // AGENT-NOTE: KWin 6.6 exposes no ListTablets; the manager's
    // `devicesSysNames` property is the only complete enumeration.
    const QDBusMessage reply =
        call(m_bus, QLatin1String(ManagerPath),
             QLatin1String(PropertiesInterface), QStringLiteral("Get"),
             {QLatin1String(ManagerInterface),
              QStringLiteral("devicesSysNames")});
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("devicesSysNames failed: %1")
                         .arg(reply.errorMessage());
        }
        return {};
    }
    if (reply.arguments().size() != 1) {
        if (error != nullptr) {
            *error = QStringLiteral("devicesSysNames reply is malformed");
        }
        return {};
    }
    const QVariant unwrapped =
        reply.arguments().at(0).value<QDBusVariant>().variant();
    if (unwrapped.userType() != QMetaType::QStringList) {
        if (error != nullptr) {
            *error = QStringLiteral("devicesSysNames reply is malformed");
        }
        return {};
    }
    const QStringList sysNames = unwrapped.toStringList();
    QList<TabletDeviceSnapshot> snapshots;
    for (const QString &sysName : sysNames) {
        // AGENT-GUARD: The authority is process-wide; a compromised or
        // future reply must not run this port into unbounded object reads.
        if (!legalSysName(sysName) || snapshots.size() >= MaxDevices) {
            continue;
        }
        TabletDeviceSnapshot snapshot;
        QString deviceError;
        if (!device(sysName, &snapshot, &deviceError)) {
            if (!deviceError.isEmpty()) {
                if (error != nullptr) {
                    *error = deviceError;
                }
                return {};
            }
            continue; // vanished mid-listing, or not a tablet
        }
        snapshots.append(snapshot);
    }
    return snapshots;
}

bool KWinTabletDevicePort::device(const QString &deviceId,
                                  TabletDeviceSnapshot *snapshot,
                                  QString *error) const {
    if (snapshot == nullptr) {
        return false;
    }
    if (!legalSysName(deviceId) || !serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral("Input authority org.kde.KWin is not "
                                    "reachable for device '%1'")
                         .arg(deviceId);
        }
        return false;
    }
    const QDBusMessage reply =
        call(m_bus, devicePath(deviceId), QLatin1String(PropertiesInterface),
             QStringLiteral("GetAll"), {QLatin1String(DeviceInterface)});
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.arguments().size() != 1) {
        // A device that vanished is not an authority failure: report absence
        // with no diagnostic so a listing can skip it and continue.
        return false;
    }
    bool demarshalled = false;
    const QVariantMap raw = demarshalMap(reply.arguments().at(0), &demarshalled);
    if (!demarshalled) {
        if (error != nullptr) {
            *error = QStringLiteral("Device %1 replied with an "
                                    "uninterpretable property map")
                         .arg(deviceId);
        }
        return false;
    }
    TabletDeviceSnapshot decoded;
    if (!snapshotFromReply(deviceId, raw, &decoded)) {
        // AGENT-GUARD: A device that does not report both tablet kinds as
        // booleans cannot be classified; presenting it would be fabricated
        // truth. Fail the whole read closed with a diagnostic.
        if (error != nullptr) {
            *error = QStringLiteral("Device %1 did not report the tablet "
                                    "kind flags; the reply is "
                                    "uninterpretable")
                         .arg(deviceId);
        }
        return false;
    }
    if (!decoded.tabletTool && !decoded.tabletPad) {
        return false; // an ordinary device, not this port's business
    }
    *snapshot = decoded;
    return true;
}

bool KWinTabletDevicePort::writeProperty(const QString &deviceId,
                                         const QString &property,
                                         const QVariant &value,
                                         QString *error) const {
    const ValueKind kind = writableKind(property);
    if (kind == ValueKind::Unknown) {
        if (error != nullptr) {
            *error =
                QStringLiteral("Unknown tablet property '%1'").arg(property);
        }
        return false;
    }
    QVariant wire;
    switch (kind) {
    case ValueKind::Boolean:
        if (value.typeId() == QMetaType::Bool) {
            wire = value;
        }
        break;
    case ValueKind::Double:
        if (value.typeId() == QMetaType::Double) {
            wire = value;
        }
        break;
    case ValueKind::UnsignedInt:
        if (value.typeId() == QMetaType::UInt ||
            (value.typeId() == QMetaType::Int && value.toInt() >= 0)) {
            wire = QVariant::fromValue(value.toUInt());
        }
        break;
    case ValueKind::Text:
        if (value.typeId() == QMetaType::QString) {
            wire = value;
        }
        break;
    case ValueKind::Area: {
        bool ok = false;
        const TabletArea area = TabletArea::fromVariant(value, &ok);
        if (ok && area.isValid()) {
            // AGENT-NOTE: KWin declares outputArea/inputArea as QRectF
            // properties, and QtDBus marshals QRectF as the `(dddd)`
            // x/y/width/height struct the interface advertises. Sending the
            // rectangle itself is the only encoding KWin demarshals.
            wire = QVariant::fromValue(
                QRectF(area.x, area.y, area.width, area.height));
        }
        break;
    }
    case ValueKind::Unknown:
        break;
    }
    if (!wire.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("Refusing to write property '%1' with a "
                                    "value of the wrong type or range")
                         .arg(property);
        }
        return false;
    }
    if (!legalSysName(deviceId) || !serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral("Input authority org.kde.KWin is not "
                                    "reachable for device '%1'")
                         .arg(deviceId);
        }
        return false;
    }
    const QDBusMessage reply =
        call(m_bus, devicePath(deviceId), QLatin1String(PropertiesInterface),
             QStringLiteral("Set"),
             {QLatin1String(DeviceInterface), property,
              QVariant::fromValue(QDBusVariant(wire))});
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("Setting %1 on %2 failed: %3")
                         .arg(property, deviceId, reply.errorMessage());
        }
        return false;
    }
    return true;
}

KWinTabletDeviceWatcher::KWinTabletDeviceWatcher(QDBusConnection bus,
                                                 QObject *parent)
    : TabletDeviceWatcher(parent), m_bus(std::move(bus)) {}

bool KWinTabletDeviceWatcher::start(QString *error) {
    if (m_started) {
        return true;
    }
    if (!m_bus.isConnected()) {
        if (error != nullptr) {
            *error = QStringLiteral("No session bus for tablet hotplug");
        }
        return false;
    }
    // AGENT-GUARD: Subscribe without a sender name. KWin may not own
    // org.kde.KWin yet when this process starts, and a sender-filtered match
    // registered too early never fires afterwards.
    const bool added = m_bus.connect(
        QString(), QLatin1String(ManagerPath), QLatin1String(ManagerInterface),
        QStringLiteral("deviceAdded"), this,
        SLOT(handleDeviceAdded(QString)));
    const bool removed = m_bus.connect(
        QString(), QLatin1String(ManagerPath), QLatin1String(ManagerInterface),
        QStringLiteral("deviceRemoved"), this,
        SLOT(handleDeviceRemoved(QString)));
    if (!added || !removed) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Could not observe KWin input hotplug signals");
        }
        return false;
    }
    m_started = true;
    return true;
}

void KWinTabletDeviceWatcher::handleDeviceAdded(const QString &sysName) {
    if (legalSysName(sysName)) {
        Q_EMIT deviceAdded(sysName);
    }
}

void KWinTabletDeviceWatcher::handleDeviceRemoved(const QString &sysName) {
    if (legalSysName(sysName)) {
        Q_EMIT deviceRemoved(sysName);
    }
}

} // namespace QindaQt::Services::TabletDevices
