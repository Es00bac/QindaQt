// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_notifications/notification_application_settings_model.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::SettingsNotifications {
namespace {

using Policy = Services::NotificationPresentationPolicy::NotificationApplicationPolicy;
using PolicyMap = Services::NotificationPresentationPolicy::PerApplicationNotificationPolicies;
using PolicyValue = Services::NotificationPresentationPolicy::PerApplicationNotificationPolicy;

constexpr int ReadbackRetryMilliseconds = 200;
constexpr int ReadbackDeadlineMilliseconds = 4'000;

} // namespace

NotificationApplicationSettingsModel::NotificationApplicationSettingsModel(
    Services::SettingsClient::SettingsClient &client,
    QVector<NotificationApplicationDescriptor> applications,
    QObject *parent)
    : QAbstractListModel(parent), m_client(client), m_applications(std::move(applications))
{
    connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, [this] { applySnapshot(true); });
    connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &NotificationApplicationSettingsModel::handleClientState);
    connect(&m_client, &Services::SettingsClient::SettingsClient::ownerChanged,
            this, &NotificationApplicationSettingsModel::handleClientState);
    connect(&m_client, &Services::SettingsClient::SettingsClient::writeInFlightChanged,
            this, &NotificationApplicationSettingsModel::stateChanged);
    connect(&m_client, &Services::SettingsClient::SettingsClient::writeAdmissionChanged,
            this, &NotificationApplicationSettingsModel::stateChanged);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
            this, &NotificationApplicationSettingsModel::handleCommit);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
            this, &NotificationApplicationSettingsModel::handleUncertain);
    m_readbackRetryTimer.setSingleShot(true);
    connect(&m_readbackRetryTimer, &QTimer::timeout, this, [this] {
        if (m_pending && m_waitingForReadback) {
            m_client.refresh();
        }
    });
    m_readbackDeadlineTimer.setSingleShot(true);
    connect(&m_readbackDeadlineTimer, &QTimer::timeout, this, [this] {
        if (!m_pending || !m_waitingForReadback) {
            return;
        }
        // AGENT-GUARD: a same-lineage snapshot below Applied's revision floor
        // is valid current state but cannot settle this write. Bound retries;
        // timeout retires uncertainty and never resubmits the policy.
        m_pending = false;
        m_waitingForReadback = false;
        m_uncertain = true;
        m_errorText = tr("The saved application notification settings could not be confirmed in time.");
        finishWrite();
        Q_EMIT stateChanged();
    });
    rebuildRows();
    applySnapshot();
}

int NotificationApplicationSettingsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant NotificationApplicationSettingsModel::data(const QModelIndex &index,
                                                     int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return row.application.name;
    case ApplicationIdRole:
        return row.application.id;
    case IconNameRole:
        return row.application.iconName;
    case InstalledRole:
        return row.installed;
    case MutedRole:
        return row.policy.muted;
    case SoundEnabledRole:
        return row.policy.soundEnabled;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationApplicationSettingsModel::roleNames() const
{
    return {{ApplicationIdRole, QByteArrayLiteral("applicationId")},
            {DisplayNameRole, QByteArrayLiteral("displayName")},
            {IconNameRole, QByteArrayLiteral("iconName")},
            {InstalledRole, QByteArrayLiteral("installed")},
            {MutedRole, QByteArrayLiteral("muted")},
            {SoundEnabledRole, QByteArrayLiteral("soundEnabled")}};
}

bool NotificationApplicationSettingsModel::canEdit() const
{
    return m_available && !m_pending &&
        m_client.canSetUserValue(
            QLatin1String(Services::NotificationPresentationPolicy::
                              NotificationPoliciesSettingsKey));
}

QString NotificationApplicationSettingsModel::statusText() const
{
    if (!m_dataError.isEmpty()) {
        return tr("Saved application notification settings are invalid. Changes are disabled.");
    }
    if (m_pending) {
        return m_waitingForReadback
            ? tr("Checking saved application notification settings…")
            : tr("Saving application notification settings…");
    }
    if (m_conflict) {
        return tr("Application notification settings changed elsewhere; current saved values are shown.");
    }
    if (m_uncertain) {
        return tr("The save result is unknown. Current confirmed values are shown; try again to replace them.");
    }
    if (!m_available) {
        return tr("The settings service is unavailable, so application notification settings cannot be changed.");
    }
    return {};
}

bool NotificationApplicationSettingsModel::requestSetMuted(
    const QString &applicationId, const bool muted)
{
    const int row = rowForId(applicationId);
    if (row < 0) {
        return false;
    }
    PolicyValue next = m_rows.at(row).policy;
    next.muted = muted;
    return requestPolicyChange(applicationId, next);
}

bool NotificationApplicationSettingsModel::requestSetSoundEnabled(
    const QString &applicationId, const bool soundEnabled)
{
    const int row = rowForId(applicationId);
    if (row < 0) {
        return false;
    }
    PolicyValue next = m_rows.at(row).policy;
    next.soundEnabled = soundEnabled;
    return requestPolicyChange(applicationId, next);
}

void NotificationApplicationSettingsModel::retry()
{
    if (m_pending) {
        return;
    }
    m_refreshRequested = true;
    m_client.refresh();
}

void NotificationApplicationSettingsModel::clearError()
{
    if (m_errorText.isEmpty() && !m_conflict && !m_uncertain) {
        return;
    }
    m_errorText.clear();
    m_conflict = false;
    m_uncertain = false;
    Q_EMIT stateChanged();
}

void NotificationApplicationSettingsModel::applySnapshot(const bool fresh)
{
    const auto &snapshot = m_client.snapshot();
    const bool ownerReady = snapshot.has_value() &&
        m_client.state() == Services::SettingsClient::ClientState::Ready &&
        snapshot->owner == m_client.currentOwner();
    PolicyMap nextPolicies;
    QString dataError;
    bool available = ownerReady;
    if (available) {
        const auto decoded = Policy::decodeSettingsValue(
            snapshot->values.value(QLatin1String(
                Services::NotificationPresentationPolicy::
                    NotificationPoliciesSettingsKey)));
        available = decoded.ok();
        if (available) {
            nextPolicies = *decoded.policies;
        } else {
            dataError = decoded.error;
        }
    }

    if (m_pending && m_waitingForReadback && fresh) {
        const bool sameLineage = snapshot.has_value() &&
            m_client.state() == Services::SettingsClient::ClientState::Ready &&
            snapshot->owner == m_client.currentOwner() &&
            snapshot->owner == m_writeOwner && snapshot->epoch == m_writeEpoch;
        if (sameLineage && snapshot->revision < m_readbackRevision) {
            // AGENT-GUARD: SettingsClient can accept a newer-than-cached
            // revision that is still below the Applied result floor. Keep the
            // intent pending and fetch again; this snapshot cannot confirm or
            // refute the write.
            if (!m_readbackRetryTimer.isActive()) {
                m_readbackRetryTimer.start(ReadbackRetryMilliseconds);
            }
        } else {
            m_pending = false;
            m_waitingForReadback = false;
            if (!available || !sameLineage) {
                m_uncertain = true;
                m_errorText = tr("The saved application notification settings could not be confirmed.");
            } else if (nextPolicies != m_requestedPolicies) {
                m_conflict = true;
                m_errorText = tr("The saved application notification settings differ from your choice.");
            } else {
                m_conflict = false;
                m_uncertain = false;
                m_errorText.clear();
            }
            finishWrite();
        }
    }

    if (fresh && available && m_refreshRequested) {
        m_refreshRequested = false;
        m_conflict = false;
        m_uncertain = false;
        m_errorText.clear();
    }

    const bool changed = available != m_available || dataError != m_dataError ||
                         (available && nextPolicies != m_confirmedPolicies);
    if (available) {
        m_hasBaseline = true;
        m_confirmedPolicies = std::move(nextPolicies);
    }
    m_available = available;
    m_dataError = std::move(dataError);
    if (changed) {
        rebuildRows();
    }
    if (changed || fresh) {
        Q_EMIT stateChanged();
    }
}

void NotificationApplicationSettingsModel::handleClientState()
{
    bool retired = false;
    if (m_pending && (m_client.currentOwner() != m_writeOwner ||
        m_client.state() == Services::SettingsClient::ClientState::Unavailable ||
        m_client.state() == Services::SettingsClient::ClientState::Degraded)) {
        m_pending = false;
        m_waitingForReadback = false;
        m_uncertain = true;
        m_errorText = tr("The settings service changed before application notification settings could be confirmed.");
        finishWrite();
        retired = true;
    }
    applySnapshot();
    if (retired) {
        Q_EMIT stateChanged();
    }
}

void NotificationApplicationSettingsModel::handleCommit(
    const Services::SettingsClient::CommitOutcome &outcome)
{
    if (!m_pending || m_waitingForReadback) {
        return;
    }
    using Services::SettingsProtocol::SettingsWireStatus;
    if (outcome.status == SettingsWireStatus::Applied) {
        m_waitingForReadback = true;
        m_readbackRevision = outcome.revisionAfter;
        m_readbackDeadlineTimer.start(ReadbackDeadlineMilliseconds);
    } else {
        m_pending = false;
        m_conflict = outcome.status == SettingsWireStatus::Conflict;
        m_errorText = outcome.message.left(512);
        if (m_errorText.isEmpty()) {
            m_errorText = tr("Application notification settings save was refused (%1).")
                .arg(Services::SettingsProtocol::settingsWireStatusName(outcome.status));
        }
        finishWrite();
    }
    Q_EMIT stateChanged();
}

void NotificationApplicationSettingsModel::handleUncertain(const QString &message)
{
    if (!m_pending) {
        return;
    }
    m_pending = false;
    m_waitingForReadback = false;
    m_uncertain = true;
    m_errorText = message.isEmpty()
        ? tr("Application notification settings save result is unknown.")
        : message.left(512);
    finishWrite();
    Q_EMIT stateChanged();
}

bool NotificationApplicationSettingsModel::requestPolicyChange(
    const QString &applicationId, const PolicyValue &policy)
{
    if (!canEdit() || rowForId(applicationId) < 0 ||
        !Policy::isCanonicalDesktopId(applicationId)) {
        return false;
    }
    PolicyMap requested = m_confirmedPolicies;
    if (!policy.muted && !policy.soundEnabled) {
        requested.remove(applicationId);
    } else {
        requested.insert(applicationId, policy);
    }
    if (requested == m_confirmedPolicies) {
        return false;
    }
    QVariantMap encoded;
    QString error;
    if (!Policy::encodeSettingsValue(requested, &encoded, &error)) {
        m_errorText = tr("Application policy limit reached. Remove another rule before adding this one.");
        Q_EMIT stateChanged();
        return false;
    }

    m_pending = true;
    m_waitingForReadback = false;
    m_conflict = false;
    m_uncertain = false;
    m_errorText.clear();
    m_writeOwner = m_client.currentOwner();
    m_writeEpoch = m_client.snapshot()->epoch;
    m_requestedPolicies = std::move(requested);
    if (m_client.setUserValue(
            QLatin1String(Services::NotificationPresentationPolicy::
                              NotificationPoliciesSettingsKey),
            encoded, &error)) {
        Q_EMIT stateChanged();
        return true;
    }
    m_pending = false;
    finishWrite();
    m_errorText = error.left(512);
    Q_EMIT stateChanged();
    return false;
}

void NotificationApplicationSettingsModel::rebuildRows()
{
    QVector<Row> next;
    next.reserve(m_applications.size() + m_confirmedPolicies.size());
    QSet<QString> installedIds;
    for (const NotificationApplicationDescriptor &application : m_applications) {
        if (!Policy::isCanonicalDesktopId(application.id) ||
            installedIds.contains(application.id)) {
            continue;
        }
        installedIds.insert(application.id);
        next.append({application, true, m_confirmedPolicies.value(application.id)});
    }
    for (auto it = m_confirmedPolicies.cbegin(); it != m_confirmedPolicies.cend(); ++it) {
        if (installedIds.contains(it.key())) {
            continue;
        }
        next.append({{it.key(), it.key(), {}}, false, it.value()});
    }

    const auto sameRows = [](const QVector<Row> &left, const QVector<Row> &right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (qsizetype index = 0; index < left.size(); ++index) {
            if (left.at(index).application != right.at(index).application ||
                left.at(index).installed != right.at(index).installed) {
                return false;
            }
        }
        return true;
    };
    if (!sameRows(m_rows, next)) {
        beginResetModel();
        m_rows = std::move(next);
        endResetModel();
        return;
    }
    if (m_rows.isEmpty()) {
        return;
    }
    for (qsizetype index = 0; index < next.size(); ++index) {
        m_rows[index].policy = next.at(index).policy;
    }
    Q_EMIT dataChanged(this->index(0, 0),
                        this->index(static_cast<int>(m_rows.size()) - 1, 0),
                        {MutedRole, SoundEnabledRole});
}

int NotificationApplicationSettingsModel::rowForId(
    const QString &applicationId) const noexcept
{
    const auto it = std::find_if(m_rows.cbegin(), m_rows.cend(),
                                 [&applicationId](const Row &row) {
                                     return row.application.id == applicationId;
                                 });
    return it == m_rows.cend() ? -1 : int(it - m_rows.cbegin());
}

void NotificationApplicationSettingsModel::finishWrite()
{
    m_readbackRetryTimer.stop();
    m_readbackDeadlineTimer.stop();
    m_writeOwner.clear();
    m_writeEpoch.clear();
    m_requestedPolicies.clear();
}

} // namespace QindaQt::Apps::SettingsNotifications
