// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_model/notification_list_model.h"

#include <QVariantList>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Services::NotificationPresentationModel {

NotificationListModel::NotificationListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int NotificationListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant NotificationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= m_entries.size()) {
        return {};
    }
    const auto &entry = m_entries.at(index.row());
    const auto &notification = entry.notification;
    switch (role) {
    case NotificationIdRole:
        return notification.id;
    case ApplicationNameRole:
        return notification.applicationName;
    case SummaryRole:
        return notification.summary;
    case BodyRole:
        return notification.body;
    case UrgencyRole:
        return notification.urgency;
    case CreatedAtRole:
        return notification.createdAtMs;
    case ActionsRole: {
        QVariantList result;
        result.reserve(notification.actions.size());
        for (const auto &action : notification.actions) {
            result.append(QVariantMap{{QStringLiteral("key"), action.key},
                                      {QStringLiteral("label"), action.label}});
        }
        return result;
    }
    case ActiveRole:
        return entry.active;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationListModel::roleNames() const
{
    return {{NotificationIdRole, QByteArrayLiteral("notificationId")},
            {ApplicationNameRole, QByteArrayLiteral("applicationName")},
            {SummaryRole, QByteArrayLiteral("summary")},
            {BodyRole, QByteArrayLiteral("body")},
            {UrgencyRole, QByteArrayLiteral("urgency")},
            {CreatedAtRole, QByteArrayLiteral("createdAt")},
            {ActionsRole, QByteArrayLiteral("actions")},
            {ActiveRole, QByteArrayLiteral("active")}};
}

void NotificationListModel::replace(QVector<NotificationListEntry> entries)
{
    if (m_entries == entries) {
        return;
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

const QVector<NotificationListEntry> &
NotificationListModel::entries() const noexcept
{
    return m_entries;
}

} // namespace QindaQt::Services::NotificationPresentationModel
