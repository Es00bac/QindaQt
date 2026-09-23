// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/pointer_device_port.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QHash>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDBusServiceWatcher>
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
constexpr int InventoryBudgetMs = 5000;

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

void PointerDevicePort::setObserving(bool active) { Q_UNUSED(active); }

void PointerDevicePort::requestDevices(QObject *receiver,
                                       DevicesReply reply) const {
    Q_UNUSED(receiver);
    QString error;
    auto snapshots = devices(&error);
    reply(std::move(snapshots), error);
}

void PointerDevicePort::requestWrite(
    QObject *receiver, const QString &deviceId,
    const QList<QPair<QString, QVariant>> &changes, WriteReply reply) const {
    Q_UNUSED(receiver);
    WriteResult result;
    result.applied = true;
    QString beforeError;
    const auto before = devices(&beforeError);
    std::optional<PointerDeviceSnapshot> original;
    if (beforeError.isEmpty()) {
        for (const auto &snapshot : before) {
            if (snapshot.deviceId == deviceId) {
                original = snapshot;
                break;
            }
        }
    }
    if (!original) {
        result.applied = false;
        result.error = beforeError.isEmpty()
                           ? QStringLiteral("Device disappeared before write")
                           : beforeError;
    }
    int completed = 0;
    if (result.applied) {
        for (const auto &change : changes) {
            if (!writeProperty(deviceId, change.first, change.second,
                               &result.error)) {
                result.applied = false;
                break;
            }
            ++completed;
        }
    }
    if (!result.applied && original) {
        // AGENT-GUARD: Profile and scroll-method choices span two KWin
        // properties. Restore earlier writes if the later Set is refused.
        for (int i = completed - 1; i >= 0; --i) {
            const auto &change = changes.at(i);
            if (original->properties.contains(change.first)) {
                writeProperty(deviceId, change.first,
                              original->properties.value(change.first),
                              nullptr);
            }
        }
    }
    QString readError;
    const auto snapshots = devices(&readError);
    if (readError.isEmpty()) {
        for (const auto &snapshot : snapshots) {
            if (snapshot.deviceId == deviceId) {
                result.snapshot = snapshot;
                break;
            }
        }
    }
    if (!result.snapshot) {
        result.applied = false;
        if (result.error.isEmpty()) {
            result.error = readError.isEmpty()
                               ? QStringLiteral("Device disappeared before readback")
                               : readError;
        }
    } else if (result.applied) {
        for (const auto &change : changes) {
            if (result.snapshot->properties.value(change.first) !=
                change.second) {
                result.applied = false;
                result.error = QStringLiteral(
                    "Input authority did not retain %1").arg(change.first);
                break;
            }
        }
    }
    reply(std::move(result));
}

KWinPointerDevicePort::KWinPointerDevicePort(QDBusConnection bus)
    : m_bus(std::move(bus)) {}

void KWinPointerDevicePort::setObserving(bool active) {
    m_propertiesObserved = active;
    if (!active || m_serviceWatcher) return;
    m_serviceWatcher = new QDBusServiceWatcher(
        QLatin1String(KWinService), m_bus,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, [this] { Q_EMIT authorityChanged(); });
    // PropertiesChanged is the authority's standard external-edit feed.
    // Polling in the model also covers manager inventory changes on KWin
    // versions that do not emit a matching pointer-list signal.
    m_bus.connect(QLatin1String(KWinService), QString(),
                  QLatin1String(PropertiesInterface),
                  QStringLiteral("PropertiesChanged"), this,
                  SLOT(handlePropertiesChanged(QString,QVariantMap,QStringList)));
}

void KWinPointerDevicePort::handlePropertiesChanged(
    const QString &interface, const QVariantMap &changed,
    const QStringList &invalidated) {
    Q_UNUSED(changed);
    Q_UNUSED(invalidated);
    if (m_propertiesObserved && interface == QLatin1String(DeviceInterface)) {
        Q_EMIT inventoryChanged();
    }
}

void KWinPointerDevicePort::requestDevices(QObject *receiver,
                                           DevicesReply reply) const {
    using Result = QPair<QList<PointerDeviceSnapshot>, QString>;
    auto *watcher = new QFutureWatcher<Result>(receiver);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, receiver,
                     [watcher, reply = std::move(reply)]() mutable {
                         auto result = watcher->result();
                         watcher->deleteLater();
                         reply(std::move(result.first), std::move(result.second));
                     });
    const QDBusConnection bus = m_bus;
    watcher->setFuture(QtConcurrent::run([bus] {
        const auto ownerBefore = bus.interface()
            ? bus.interface()->serviceOwner(QLatin1String(KWinService)).value()
            : QString();
        QString error;
        KWinPointerDevicePort probe(bus);
        auto snapshots = probe.devices(&error);
        const auto ownerAfter = bus.interface()
            ? bus.interface()->serviceOwner(QLatin1String(KWinService)).value()
            : QString();
        if (ownerBefore != ownerAfter) {
            error = QStringLiteral("Input authority changed during refresh");
            snapshots.clear();
        }
        return Result{std::move(snapshots), std::move(error)};
    }));
}

void KWinPointerDevicePort::requestWrite(
    QObject *receiver, const QString &deviceId,
    const QList<QPair<QString, QVariant>> &changes, WriteReply reply) const {
    auto *watcher = new QFutureWatcher<WriteResult>(receiver);
    QObject::connect(watcher, &QFutureWatcher<WriteResult>::finished, receiver,
                     [watcher, reply = std::move(reply)]() mutable {
                         auto result = watcher->result();
                         watcher->deleteLater();
                         reply(std::move(result));
                     });
    const QDBusConnection bus = m_bus;
    watcher->setFuture(QtConcurrent::run([bus, deviceId, changes] {
        const auto ownerBefore = bus.interface()
            ? bus.interface()->serviceOwner(QLatin1String(KWinService)).value()
            : QString();
        KWinPointerDevicePort probe(bus);
        WriteResult result;
        probe.PointerDevicePort::requestWrite(
            nullptr, deviceId, changes,
            [&result](WriteResult completed) { result = std::move(completed); });
        const auto ownerAfter = bus.interface()
            ? bus.interface()->serviceOwner(QLatin1String(KWinService)).value()
            : QString();
        if (ownerBefore != ownerAfter) {
            result.applied = false;
            result.snapshot.reset();
            result.error = QStringLiteral("Input authority changed during write");
        }
        return result;
    }));
}

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
    QElapsedTimer elapsed;
    elapsed.start();
    for (const QString &sysName : sysNames) {
        if (elapsed.elapsed() >= InventoryBudgetMs) {
            if (error) {
                *error = QStringLiteral("Pointer inventory timed out");
            }
            return {};
        }
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
                 QStringLiteral("tapFingerCount"),
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
                 QStringLiteral("supportsDisableWhileTyping"),
                 QStringLiteral("supportsMiddleEmulation"),
                 QStringLiteral("supportsLmrTapButtonMap"),
                 QStringLiteral("supportsScrollTwoFinger"),
                 QStringLiteral("supportsScrollEdge"),
                 QStringLiteral("supportsScrollOnButtonDown"),
                 // AGENT-NOTE: KWin's real org.kde.KWin.InputDevice interface
                 // has no supportsTapToClick/supportsTapAndDrag/default<Prop>
                 // properties; it names the "on by default" family
                 // "<prop>EnabledByDefault" and signals tap capability
                 // through the integer tapFingerCount (0 = no tap support),
                 // confirmed against the live qinda-top touchpad
                 // (event4, SYNA2BA6). Reading the names this file used to
                 // use left tapToClickAvailable/tapAndDragAvailable always
                 // false, hiding the whole Touchpad section.
                 QStringLiteral("leftHandedEnabledByDefault"),
                 QStringLiteral("naturalScrollEnabledByDefault"),
                 QStringLiteral("defaultPointerAcceleration"),
                 QStringLiteral("defaultPointerAccelerationProfileFlat"),
                 QStringLiteral("defaultPointerAccelerationProfileAdaptive"),
                 QStringLiteral("tapToClickEnabledByDefault"),
                 QStringLiteral("tapAndDragEnabledByDefault"),
                 QStringLiteral("disableWhileTypingEnabledByDefault"),
                 QStringLiteral("middleEmulationEnabledByDefault"),
                 QStringLiteral("scrollTwoFingerEnabledByDefault"),
                 QStringLiteral("scrollEdgeEnabledByDefault"),
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
