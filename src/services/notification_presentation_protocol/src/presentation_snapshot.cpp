// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"

#include "qindaqt/services/notification_presentation/wire_contract.h"

#include <QMetaType>
#include <QSet>
#include <QUuid>
#include <QDBusArgument>
#include <QDBusVariant>

namespace QindaQt::Services::NotificationPresentation {
namespace {

const QSet<QString> SnapshotKeys = {
    QStringLiteral("schemaVersion"), QStringLiteral("epoch"),
    QStringLiteral("revision"), QStringLiteral("notifications")};
const QSet<QString> NotificationKeys = {
    QStringLiteral("id"), QStringLiteral("applicationName"),
    QStringLiteral("applicationIcon"), QStringLiteral("summary"),
    QStringLiteral("body"), QStringLiteral("urgency"),
    QStringLiteral("desktopEntry"), QStringLiteral("imagePath"),
    QStringLiteral("resident"), QStringLiteral("transient"),
    QStringLiteral("createdAtMs"), QStringLiteral("updatedAtMs"),
    QStringLiteral("expiresAtMs"), QStringLiteral("actions")};
const QSet<QString> ActionKeys = {QStringLiteral("key"), QStringLiteral("label")};

SnapshotDecodeResult failure(QString error)
{
    return {std::nullopt, std::move(error)};
}

bool exactKeys(const QVariantMap &map, const QSet<QString> &expected)
{
    return QSet<QString>(map.keyBegin(), map.keyEnd()) == expected;
}

bool exactType(const QVariant &value, QMetaType::Type type)
{
    return value.isValid() && value.metaType().id() == type;
}

std::optional<QVariantList> variantList(const QVariant &value,
                                        qsizetype maximumElements)
{
    if (exactType(value, QMetaType::QVariantList)) {
        const QVariantList result = value.toList();
        return result.size() <= maximumElements
            ? std::optional<QVariantList>(result)
            : std::nullopt;
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
        if (argument.currentSignature() != QLatin1String("av")) {
            return std::nullopt;
        }
        QVariantList result;
        argument.beginArray();
        while (!argument.atEnd()) {
            if (result.size() >= maximumElements) {
                return std::nullopt;
            }
            QDBusVariant item;
            argument >> item;
            result.append(item.variant());
        }
        argument.endArray();
        return result;
    }
    return std::nullopt;
}

std::optional<QVariantMap> variantMap(const QVariant &value,
                                      qsizetype maximumEntries)
{
    if (exactType(value, QMetaType::QVariantMap)) {
        const QVariantMap result = value.toMap();
        return result.size() <= maximumEntries
            ? std::optional<QVariantMap>(result)
            : std::nullopt;
    }
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
        if (argument.currentSignature() != QLatin1String("a{sv}")) {
            return std::nullopt;
        }
        QVariantMap result;
        argument.beginMap();
        while (!argument.atEnd()) {
            if (result.size() >= maximumEntries) {
                return std::nullopt;
            }
            QString key;
            QDBusVariant item;
            argument.beginMapEntry();
            argument >> key >> item;
            argument.endMapEntry();
            if (result.contains(key)) {
                return std::nullopt;
            }
            result.insert(std::move(key), item.variant());
        }
        argument.endMap();
        return result;
    }
    return std::nullopt;
}

bool validText(const QString &value, qsizetype maximumBytes, bool allowEmpty,
               qsizetype *budget)
{
    if ((!allowEmpty && value.isEmpty()) || value.contains(QChar::Null)) {
        return false;
    }
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character.isHighSurrogate()) {
            if (++index >= value.size() || !value.at(index).isLowSurrogate()) {
                return false;
            }
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    const qsizetype bytes = value.toUtf8().size();
    if (bytes > maximumBytes || *budget > WireContract::MaximumSnapshotTextBytes - bytes) {
        return false;
    }
    *budget += bytes;
    return true;
}

std::optional<qint64> optionalTime(const QVariant &value, bool *valid)
{
    if (!exactType(value, QMetaType::LongLong)) {
        *valid = false;
        return std::nullopt;
    }
    const qint64 decoded = value.toLongLong();
    if (decoded < -1) {
        *valid = false;
        return std::nullopt;
    }
    if (decoded == -1) {
        return std::nullopt;
    }
    return decoded;
}

QVariantMap encodeAction(const PresentationAction &action)
{
    return {{QStringLiteral("key"), action.key},
            {QStringLiteral("label"), action.label}};
}

QVariantMap encodeNotification(const PresentationNotification &notification)
{
    QVariantList actions;
    actions.reserve(notification.actions.size());
    for (const auto &action : notification.actions) {
        actions.append(encodeAction(action));
    }
    return {{QStringLiteral("id"), notification.id},
            {QStringLiteral("applicationName"), notification.applicationName},
            {QStringLiteral("applicationIcon"), notification.applicationIcon},
            {QStringLiteral("summary"), notification.summary},
            {QStringLiteral("body"), notification.body},
            {QStringLiteral("urgency"), notification.urgency},
            {QStringLiteral("desktopEntry"), notification.desktopEntry},
            {QStringLiteral("imagePath"), notification.imagePath},
            {QStringLiteral("resident"), notification.resident},
            {QStringLiteral("transient"), notification.transient},
            {QStringLiteral("createdAtMs"), notification.createdAtMs},
            {QStringLiteral("updatedAtMs"),
             notification.updatedAtMs.value_or(qint64(-1))},
            {QStringLiteral("expiresAtMs"),
             notification.expiresAtMs.value_or(qint64(-1))},
            {QStringLiteral("actions"), actions}};
}

} // namespace

QVariantMap PresentationSnapshotCodec::encode(const PresentationSnapshot &snapshot)
{
    QVariantList notifications;
    notifications.reserve(snapshot.notifications.size());
    for (const auto &notification : snapshot.notifications) {
        notifications.append(encodeNotification(notification));
    }
    return {{QStringLiteral("schemaVersion"), WireContract::SchemaVersion},
            {QStringLiteral("epoch"), snapshot.epoch},
            {QStringLiteral("revision"), snapshot.revision},
            {QStringLiteral("notifications"), notifications}};
}

SnapshotDecodeResult PresentationSnapshotCodec::decode(const QVariantMap &wire)
{
    if (!exactKeys(wire, SnapshotKeys) ||
        !exactType(wire.value(QStringLiteral("schemaVersion")), QMetaType::UInt) ||
        wire.value(QStringLiteral("schemaVersion")).toUInt() != WireContract::SchemaVersion ||
        !exactType(wire.value(QStringLiteral("epoch")), QMetaType::QString) ||
        !exactType(wire.value(QStringLiteral("revision")), QMetaType::ULongLong)) {
        return failure(QStringLiteral("notification presentation snapshot envelope is invalid"));
    }

    PresentationSnapshot snapshot;
    snapshot.epoch = wire.value(QStringLiteral("epoch")).toString();
    const QUuid epoch(snapshot.epoch);
    if (epoch.isNull() || epoch.toString(QUuid::WithoutBraces) != snapshot.epoch) {
        return failure(QStringLiteral("notification presentation epoch is not canonical"));
    }
    snapshot.revision = wire.value(QStringLiteral("revision")).toULongLong();
    const auto decodedItems = variantList(wire.value(QStringLiteral("notifications")),
                                          WireContract::MaximumNotifications);
    if (!decodedItems) {
        return failure(QStringLiteral("notification presentation items are not a list"));
    }
    const QVariantList &items = *decodedItems;
    if (items.size() > WireContract::MaximumNotifications) {
        return failure(QStringLiteral("notification presentation snapshot exceeds its item limit"));
    }

    qsizetype textBudget = 0;
    quint32 previousId = 0;
    snapshot.notifications.reserve(items.size());
    for (const QVariant &itemValue : items) {
        const auto decodedItem = variantMap(itemValue, NotificationKeys.size());
        if (!decodedItem) {
            return failure(QStringLiteral("notification presentation item is not an object"));
        }
        const QVariantMap &item = *decodedItem;
        if (!exactKeys(item, NotificationKeys)) {
            return failure(QStringLiteral("notification presentation item fields are invalid"));
        }
        const auto stringValue = [&item, &textBudget](const QString &key,
                                                      qsizetype maximum,
                                                      bool allowEmpty,
                                                      QString *output) {
            const QVariant value = item.value(key);
            if (!exactType(value, QMetaType::QString)) {
                return false;
            }
            *output = value.toString();
            return validText(*output, maximum, allowEmpty, &textBudget);
        };
        if (!exactType(item.value(QStringLiteral("id")), QMetaType::UInt) ||
            !exactType(item.value(QStringLiteral("urgency")), QMetaType::UInt) ||
            !exactType(item.value(QStringLiteral("resident")), QMetaType::Bool) ||
            !exactType(item.value(QStringLiteral("transient")), QMetaType::Bool) ||
            !exactType(item.value(QStringLiteral("createdAtMs")), QMetaType::LongLong)) {
            return failure(QStringLiteral("notification presentation item types are invalid"));
        }

        PresentationNotification notification;
        notification.id = item.value(QStringLiteral("id")).toUInt();
        notification.urgency = item.value(QStringLiteral("urgency")).toUInt();
        notification.resident = item.value(QStringLiteral("resident")).toBool();
        notification.transient = item.value(QStringLiteral("transient")).toBool();
        notification.createdAtMs = item.value(QStringLiteral("createdAtMs")).toLongLong();
        if (notification.id == 0 || notification.id <= previousId ||
            notification.urgency > 2 || notification.createdAtMs < 0 ||
            !stringValue(QStringLiteral("applicationName"),
                         WireContract::MaximumApplicationNameBytes, true,
                         &notification.applicationName) ||
            !stringValue(QStringLiteral("applicationIcon"),
                         WireContract::MaximumIconBytes, true,
                         &notification.applicationIcon) ||
            !stringValue(QStringLiteral("summary"), WireContract::MaximumSummaryBytes,
                         true, &notification.summary) ||
            !stringValue(QStringLiteral("body"), WireContract::MaximumBodyBytes,
                         true, &notification.body) ||
            !stringValue(QStringLiteral("desktopEntry"),
                         WireContract::MaximumMetadataTextBytes, true,
                         &notification.desktopEntry) ||
            !stringValue(QStringLiteral("imagePath"),
                         WireContract::MaximumMetadataTextBytes, true,
                         &notification.imagePath)) {
            return failure(QStringLiteral("notification presentation item value is invalid"));
        }
        previousId = notification.id;

        bool timesValid = true;
        notification.updatedAtMs = optionalTime(
            item.value(QStringLiteral("updatedAtMs")), &timesValid);
        notification.expiresAtMs = optionalTime(
            item.value(QStringLiteral("expiresAtMs")), &timesValid);
        if (!timesValid ||
            (notification.updatedAtMs && *notification.updatedAtMs < notification.createdAtMs) ||
            (notification.expiresAtMs && *notification.expiresAtMs < notification.createdAtMs)) {
            return failure(QStringLiteral("notification presentation timestamps are invalid"));
        }

        const auto decodedActions = variantList(item.value(QStringLiteral("actions")),
                                                WireContract::MaximumActions);
        if (!decodedActions) {
            return failure(QStringLiteral("notification presentation actions are not a list"));
        }
        const QVariantList &actionValues = *decodedActions;
        if (actionValues.size() > WireContract::MaximumActions) {
            return failure(QStringLiteral("notification presentation action limit is exceeded"));
        }
        QSet<QString> actionKeys;
        notification.actions.reserve(actionValues.size());
        for (const QVariant &actionValue : actionValues) {
            const auto decodedAction = variantMap(actionValue, ActionKeys.size());
            if (!decodedAction) {
                return failure(QStringLiteral("notification presentation action is not an object"));
            }
            const QVariantMap &actionMap = *decodedAction;
            if (!exactKeys(actionMap, ActionKeys)) {
                return failure(QStringLiteral("notification presentation action fields are invalid"));
            }
            PresentationAction action;
            const auto actionString = [&actionMap, &textBudget](const QString &key,
                                                                qsizetype maximum,
                                                                QString *output) {
                const QVariant value = actionMap.value(key);
                if (!exactType(value, QMetaType::QString)) {
                    return false;
                }
                *output = value.toString();
                return validText(*output, maximum, false, &textBudget);
            };
            if (!actionString(QStringLiteral("key"),
                              WireContract::MaximumActionKeyBytes, &action.key) ||
                !actionString(QStringLiteral("label"),
                              WireContract::MaximumActionLabelBytes, &action.label) ||
                actionKeys.contains(action.key)) {
                return failure(QStringLiteral("notification presentation action is invalid"));
            }
            actionKeys.insert(action.key);
            notification.actions.append(std::move(action));
        }
        snapshot.notifications.append(std::move(notification));
    }
    return {std::move(snapshot), {}};
}

} // namespace QindaQt::Services::NotificationPresentation
