// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation_policy/notification_application_policy.h"

#include <QAbstractListModel>
#include <QObject>
#include <QTimer>
#include <QString>
#include <QVector>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsNotifications {

struct NotificationApplicationDescriptor final {
    QString id;
    QString name;
    QString iconName;

    [[nodiscard]] bool operator==(const NotificationApplicationDescriptor &) const = default;
};

// AGENT-CONTRACT: the composition root supplies discovered desktop entries;
// this model copies them and borrows a same-thread SettingsClient that must
// outlive it. QObject parent ownership is optional. It publishes only exact
// confirmed Settings1 values; a request returning true entered the async write
// lane, not necessarily storage, and refusals/uncertain outcomes retain those
// values while status/error properties explain the result. The model does not
// read XDG paths or launch application code.
class NotificationApplicationSettingsModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool pending READ pending NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool conflict READ conflict NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool uncertain READ uncertain NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged FINAL)

public:
    enum Role {
        ApplicationIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        IconNameRole,
        InstalledRole,
        MutedRole,
        SoundEnabledRole,
    };
    Q_ENUM(Role)

    explicit NotificationApplicationSettingsModel(
        Services::SettingsClient::SettingsClient &client,
        QVector<NotificationApplicationDescriptor> applications,
        QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool available() const noexcept { return m_available; }
    [[nodiscard]] bool canEdit() const;
    [[nodiscard]] bool pending() const noexcept { return m_pending; }
    [[nodiscard]] bool conflict() const noexcept { return m_conflict; }
    [[nodiscard]] bool uncertain() const noexcept { return m_uncertain; }
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const { return m_errorText; }
    [[nodiscard]] bool hasBaseline() const noexcept { return m_hasBaseline; }

    Q_INVOKABLE bool requestSetMuted(const QString &applicationId, bool muted);
    Q_INVOKABLE bool requestSetSoundEnabled(const QString &applicationId,
                                            bool soundEnabled);
    Q_INVOKABLE void retry();
    Q_INVOKABLE void clearError();

Q_SIGNALS:
    void stateChanged();

private:
    struct Row final {
        NotificationApplicationDescriptor application;
        bool installed = true;
        Services::NotificationPresentationPolicy::PerApplicationNotificationPolicy policy;
    };

    void applySnapshot(bool fresh = false);
    void handleClientState();
    void handleCommit(const Services::SettingsClient::CommitOutcome &outcome);
    void handleUncertain(const QString &message);
    [[nodiscard]] bool requestPolicyChange(const QString &applicationId,
                                          const Services::NotificationPresentationPolicy::PerApplicationNotificationPolicy &policy);
    void rebuildRows();
    [[nodiscard]] int rowForId(const QString &applicationId) const noexcept;
    void finishWrite();

    Services::SettingsClient::SettingsClient &m_client;
    QVector<NotificationApplicationDescriptor> m_applications;
    QVector<Row> m_rows;
    Services::NotificationPresentationPolicy::PerApplicationNotificationPolicies
        m_confirmedPolicies;
    Services::NotificationPresentationPolicy::PerApplicationNotificationPolicies
        m_requestedPolicies;
    QString m_errorText;
    QString m_dataError;
    QString m_writeOwner;
    QString m_writeEpoch;
    quint64 m_readbackRevision = 0;
    QTimer m_readbackRetryTimer;
    QTimer m_readbackDeadlineTimer;
    bool m_available = false;
    bool m_hasBaseline = false;
    bool m_pending = false;
    bool m_waitingForReadback = false;
    bool m_conflict = false;
    bool m_uncertain = false;
    bool m_refreshRequested = false;
};

} // namespace QindaQt::Apps::SettingsNotifications
