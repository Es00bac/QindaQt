// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/notification_policy.h>
#include <QDBusMetaType>
#include <QSet>
#include <cmath>
namespace QindaQt::Services::Portal {
QDBusArgument &operator<<(QDBusArgument &a, const SerializedIcon &v) { a.beginStructure(); a << v.kind << v.value; a.endStructure(); return a; }
const QDBusArgument &operator>>(const QDBusArgument &a, SerializedIcon &v) { a.beginStructure(); a >> v.kind >> v.value; a.endStructure(); return a; }
void registerNotificationTypes() { qDBusRegisterMetaType<SerializedIcon>(); qDBusRegisterMetaType<QList<QVariantMap>>(); }
namespace {
bool text(const QString &s, qsizetype maximum) {
    if (s.size() > maximum) return false;
    for (const auto c : s) if (c.isNull() || (c.category() == QChar::Other_Control && c != QLatin1Char('\n'))) return false;
    return true;
}
bool identity(const QString &s, qsizetype maximum) {
    if (!text(s, maximum)) return false;
    for (const auto c : s) if (c.category() == QChar::Other_Control) return false;
    return true;
}
bool target(const QVariant &v, int depth = 0) {
    if (depth > 4) return false;
    const auto type = v.metaType();
    if (type == QMetaType::fromType<QString>()) return text(v.toString(), 4096);
    if (type == QMetaType::fromType<QByteArray>()) return v.toByteArray().size() <= 4096;
    if (type == QMetaType::fromType<bool>() || type == QMetaType::fromType<int>()
        || type == QMetaType::fromType<uint>() || type == QMetaType::fromType<qlonglong>()
        || type == QMetaType::fromType<qulonglong>()) return true;
    if (type == QMetaType::fromType<double>()) return std::isfinite(v.toDouble());
    if (type == QMetaType::fromType<QStringList>()) {
        const auto values = v.toStringList(); if (values.size() > 16) return false;
        for (const auto &value : values) if (!text(value, 256)) return false;
        return true;
    }
    // Arbitrary opaque/container targets are not needed for the supported
    // plain actions; refusing them keeps retained action payloads bounded.
    return false;
}
QVariant unwrap(const QVariant &v) { return v.metaType() == QMetaType::fromType<QDBusVariant>() ? v.value<QDBusVariant>().variant() : v; }
}
std::optional<PortalNotification> portalNotification(const QString &app, const QString &id, const QVariantMap &map) {
    if (!identity(app, 255) || id.isEmpty() || !identity(id, 255) || map.size() > 32) return {};
    PortalNotification result; result.appId = app; result.id = id;
    for (const auto &key : {QStringLiteral("title"), QStringLiteral("body"), QStringLiteral("priority"),
        QStringLiteral("category"), QStringLiteral("default-action")})
        if (map.contains(key) && (map[key].metaType() != QMetaType::fromType<QString>() || !text(map[key].toString(), key == QStringLiteral("body") ? 4096 : 512))) return {};
    result.title = map.value(QStringLiteral("title"), QStringLiteral("Notification")).toString();
    result.body = map.value(QStringLiteral("body")).toString();
    const auto priority = map.value(QStringLiteral("priority"), QStringLiteral("normal")).toString();
    if (!QStringList{QStringLiteral("low"), QStringLiteral("normal"), QStringLiteral("high"), QStringLiteral("urgent")}.contains(priority)) return {};
    const uchar urgency = priority == QStringLiteral("low") ? 0 : priority == QStringLiteral("urgent") ? 2 : 1;
    result.hints.insert(QStringLiteral("urgency"), QVariant::fromValue(urgency));
    result.hints.insert(QStringLiteral("desktop-entry"), app);
    result.hints.insert(QStringLiteral("category"), map.value(QStringLiteral("category")).toString());
    if (map.contains(QStringLiteral("display-hint"))) {
        const auto value = map[QStringLiteral("display-hint")]; QStringList hints;
        if (value.metaType() == QMetaType::fromType<QStringList>()) hints = value.toStringList();
        else if (value.metaType() == QMetaType::fromType<QDBusArgument>() && value.value<QDBusArgument>().currentSignature() == QStringLiteral("as")) hints = qdbus_cast<QStringList>(value);
        else return {};
        if (hints.size() > 16) return {};
        for (const auto &hint : hints) if (!text(hint, 128)) return {};
        if (hints.contains(QStringLiteral("transient")) && hints.contains(QStringLiteral("tray"))) return {};
        result.hints.insert(QStringLiteral("transient"), hints.contains(QStringLiteral("transient")));
        // Persistent/tray/lockscreen flags are not advertised as implemented
        // native policy. Unknown optional hints do not fabricate enforcement.
    }
    if (map.contains(QStringLiteral("sound"))) {
        const auto sound = unwrap(map[QStringLiteral("sound")]);
        if (sound.metaType() != QMetaType::fromType<QString>()
            || (sound.toString() != QStringLiteral("default") && sound.toString() != QStringLiteral("silent"))) return {};
        result.hints.insert(QStringLiteral("suppress-sound"), sound.toString() == QStringLiteral("silent"));
    }
    if (map.contains(QStringLiteral("icon"))) {
        const auto icon = unwrap(map[QStringLiteral("icon")]); SerializedIcon serialized;
        if (icon.metaType() == QMetaType::fromType<QString>()) result.icon = icon.toString();
        else {
            if (icon.metaType() == QMetaType::fromType<SerializedIcon>()) serialized = icon.value<SerializedIcon>();
            else if (icon.metaType() == QMetaType::fromType<QDBusArgument>() && icon.value<QDBusArgument>().currentSignature() == QStringLiteral("(sv)")) serialized = qdbus_cast<SerializedIcon>(icon);
            else return {};
            const auto payload = serialized.value.variant();
            if (serialized.kind == QStringLiteral("themed")) {
                QStringList names;
                if (payload.metaType() == QMetaType::fromType<QStringList>()) names = payload.toStringList();
                else if (payload.metaType() == QMetaType::fromType<QDBusArgument>() && payload.value<QDBusArgument>().currentSignature() == QStringLiteral("as")) names = qdbus_cast<QStringList>(payload);
                else return {};
                if (names.isEmpty() || names.size() > 16) return {};
                for (const auto &name : names) if (!text(name, 255) || name.contains(QLatin1Char('/'))) return {};
                result.icon = names.first();
            } else if (serialized.kind == QStringLiteral("file-descriptor") && payload.metaType() == QMetaType::fromType<QDBusUnixFileDescriptor>()) result.image = payload.value<QDBusUnixFileDescriptor>();
            else return {}; // Version2 forbids legacy raw bytes icons.
        }
        if (!text(result.icon, 255) || result.icon.contains(QLatin1Char('/'))) return {};
    }
    auto action = [&result](const QString &key, const QString &name, const QString &label, const QVariant &value) {
        if (name.isEmpty() || !text(name, 255) || !text(label, 512) || (value.isValid() && !target(unwrap(value)))) return false;
        if (result.appId.isEmpty() && name.startsWith(QStringLiteral("app."))) return false;
        result.actions.append(key); result.actions.append(label);
        result.actionValues.insert(key, {name, value.isValid() ? unwrap(value) : QVariant{}}); return true;
    };
    if (map.contains(QStringLiteral("default-action")) && !action(QStringLiteral("default"), map[QStringLiteral("default-action")].toString(), {}, map.value(QStringLiteral("default-action-target")))) return {};
    if (map.contains(QStringLiteral("buttons"))) {
        const auto value = map[QStringLiteral("buttons")]; QList<QVariantMap> buttons;
        if (value.metaType() == QMetaType::fromType<QList<QVariantMap>>()) buttons = value.value<QList<QVariantMap>>();
        else if (value.metaType() == QMetaType::fromType<QDBusArgument>() && value.value<QDBusArgument>().currentSignature() == QStringLiteral("aa{sv}")) buttons = qdbus_cast<QList<QVariantMap>>(value);
        else return {};
        if (buttons.size() > 8) return {};
        for (qsizetype i = 0; i < buttons.size(); ++i) {
            const auto &button = buttons[i]; if (button.size() > 8) return {};
            if (!button.contains(QStringLiteral("label"))) continue; // Unsupported purpose may omit label.
            if (button.value(QStringLiteral("label")).metaType() != QMetaType::fromType<QString>()
                || button.value(QStringLiteral("action")).metaType() != QMetaType::fromType<QString>()) return {};
            if (!action(QStringLiteral("button%1").arg(i), button.value(QStringLiteral("action")).toString(),
                button.value(QStringLiteral("label")).toString(), button.value(QStringLiteral("target")))) return {};
        }
    }
    return result;
}
} // namespace QindaQt::Services::Portal
