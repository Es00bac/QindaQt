// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_snapshot.h"

#include <QAbstractListModel>
#include <QVector>

namespace QindaQt::Services::NotificationPresentationModel {

struct NotificationListEntry final {
    NotificationPresentation::PresentationNotification notification;
    bool active = true;

    bool operator==(const NotificationListEntry &) const = default;
};

class NotificationListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NotificationIdRole = Qt::UserRole + 1,
        ApplicationNameRole,
        SummaryRole,
        BodyRole,
        UrgencyRole,
        CreatedAtRole,
        ActionsRole,
        ActiveRole,
    };
    Q_ENUM(Role)

    explicit NotificationListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // AGENT-CONTRACT: presentation controllers mutate this model only on its
    // owning thread; shell/QML consumers receive it as a read-only projection.
    void replace(QVector<NotificationListEntry> entries);
    [[nodiscard]] const QVector<NotificationListEntry> &entries() const noexcept;

private:
    QVector<NotificationListEntry> m_entries;
};

} // namespace QindaQt::Services::NotificationPresentationModel
