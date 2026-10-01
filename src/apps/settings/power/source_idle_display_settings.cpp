// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_power/source_idle_display_settings.h>

#include <qindaqt/session/idle_policy/source_preferences.h>

#include <QMetaType>

namespace QindaQt::Apps::SettingsPower {
namespace {
using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsProtocol::SettingsWireStatus;
using Session::IdlePolicy::PowerSourceProfile;

QStringList sources()
{
    return {QStringLiteral("ac"), QStringLiteral("battery"),
            QStringLiteral("lowBattery")};
}
QString sourceLabel(const QString &source)
{
    if (source == QLatin1String("ac")) return QObject::tr("On AC power");
    if (source == QLatin1String("battery")) return QObject::tr("On battery");
    return QObject::tr("On low battery");
}
bool exactInteger(const QVariant &value, qint64 *number)
{
    const int type = value.metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt &&
        type != QMetaType::LongLong && type != QMetaType::ULongLong)
        return false;
    bool ok = false;
    const qint64 candidate = value.toLongLong(&ok);
    if (!ok) return false;
    *number = candidate;
    return true;
}
} // namespace

SourceIdleDisplaySettingsModel::SourceIdleDisplaySettingsModel(
    Power::PowerClient &power,
    Services::SettingsClient::SettingsClient &settings,
    QObject *parent)
    : QObject(parent), m_power(power), m_settings(settings)
{
    m_readbackTimeout.setSingleShot(true);
    m_readbackTimeout.setInterval(5'000);
    connect(&m_readbackTimeout, &QTimer::timeout, this, [this] {
        if (m_pending && m_waitingForReadback) {
            markUncertain(tr("Settings accepted the change, but a fresh per-source value could not be confirmed."));
            m_settings.refresh();
        }
    });
    connect(&m_settings, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, [this] {
                ++m_snapshotSequence;
                applySnapshot(true);
            });
    connect(&m_settings, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &SourceIdleDisplaySettingsModel::handleClientState);
    connect(&m_settings, &Services::SettingsClient::SettingsClient::ownerChanged,
            this, &SourceIdleDisplaySettingsModel::handleClientState);
    connect(&m_settings, &Services::SettingsClient::SettingsClient::writeAdmissionChanged,
            this, &SourceIdleDisplaySettingsModel::publish);
    connect(&m_settings, &Services::SettingsClient::SettingsClient::commitFinished,
            this, &SourceIdleDisplaySettingsModel::handleCommit);
    connect(&m_settings, &Services::SettingsClient::SettingsClient::commitUncertain,
            this, &SourceIdleDisplaySettingsModel::markUncertain);
    connect(&m_power, &Power::PowerClient::snapshotChanged,
            this, &SourceIdleDisplaySettingsModel::updatePowerSource);
    connect(&m_power, &Power::PowerClient::stateChanged,
            this, [this] { updatePowerSource(); });
    applySnapshot(false);
    updatePowerSource();
}

QString SourceIdleDisplaySettingsModel::key(const QString &source,
                                             const QString &field)
{
    return QStringLiteral("power.idle.%1.%2").arg(source, field);
}

bool SourceIdleDisplaySettingsModel::validSource(const QString &source)
{
    return sources().contains(source);
}

QVariantList SourceIdleDisplaySettingsModel::sourceRows() const
{
    QVariantList rows;
    for (const QString &source : sources()) {
        const auto values = m_values.value(source);
        const bool effectiveEnabled = values.confirmed && values.enabled &&
                                      values.seconds > 0;
        rows.push_back(QVariantMap{
            {QStringLiteral("source"), source},
            {QStringLiteral("label"), sourceLabel(source)},
            {QStringLiteral("enabled"), values.enabled},
            {QStringLiteral("seconds"), values.seconds},
            {QStringLiteral("confirmed"), values.confirmed},
            {QStringLiteral("active"), source == m_activeSource},
            {QStringLiteral("sourceLayer"), values.enabledSource == values.secondsSource
                  ? values.enabledSource : tr("mixed sources")},
            {QStringLiteral("effectiveEnabled"), effectiveEnabled},
        });
    }
    return rows;
}

QString SourceIdleDisplaySettingsModel::activeSource() const
{
    return m_activeSource;
}

void SourceIdleDisplaySettingsModel::updatePowerSource()
{
    QString next;
    if (m_power.hasSnapshot() &&
        (m_power.state() == Power::PowerClientState::Ready ||
         m_power.state() == Power::PowerClientState::Degraded)) {
        const auto profile = Session::IdlePolicy::selectPowerSourceProfile(
            m_power.snapshot());
        if (profile) next = Session::IdlePolicy::powerSourceProfileKey(*profile);
    }
    if (next == m_activeSource) return;
    m_activeSource = next;
    publish();
}

void SourceIdleDisplaySettingsModel::applySnapshot(const bool fresh)
{
    const auto &snapshot = m_settings.snapshot();
    bool accepted = snapshot.has_value() &&
        m_settings.state() == ClientState::Ready &&
        snapshot->owner == m_settings.currentOwner() &&
        !snapshot->epoch.isEmpty() && snapshot->revision != 0;
    QHash<QString, Values> nextValues;
    if (accepted) {
        for (const QString &source : sources()) {
            const QVariant enabledValue = snapshot->values.value(key(source, QStringLiteral("displayOffEnabled")));
            qint64 seconds = 0;
            if (enabledValue.metaType().id() != QMetaType::Bool ||
                !exactInteger(snapshot->values.value(key(source, QStringLiteral("displayOffSeconds"))), &seconds) ||
                seconds < 0 || seconds > 14400) {
                accepted = false;
                break;
            }
            nextValues.insert(source, Values{
                .confirmed = true,
                .enabled = enabledValue.toBool(),
                .seconds = static_cast<int>(seconds),
                .enabledSource = snapshot->sourceLayers.value(
                    key(source, QStringLiteral("displayOffEnabled"))).toString(),
                .secondsSource = snapshot->sourceLayers.value(
                    key(source, QStringLiteral("displayOffSeconds"))).toString(),
            });
        }
    }
    m_available = accepted;
    if (accepted) m_values = std::move(nextValues);

    if (m_pending && m_waitingForReadback && fresh) {
        if (!accepted || snapshot->owner != m_pendingOwner ||
            snapshot->epoch != m_pendingEpoch) {
            markUncertain(tr("Settings changed before the per-source value could be confirmed."));
        } else if (m_snapshotSequence >= m_requiredSnapshotSequence &&
                   snapshot->revision >= m_revisionFloor) {
            m_pending = false;
            m_waitingForReadback = false;
            m_readbackTimeout.stop();
            if (snapshot->values.value(m_pendingKey) == m_requestedValue) {
                m_errorText.clear();
            } else {
                m_errorText = tr("The saved per-source value differs from your choice.");
            }
            m_pendingKey.clear();
            m_pendingOwner.clear();
            m_pendingEpoch.clear();
        }
    }
    publish();
}

void SourceIdleDisplaySettingsModel::handleClientState()
{
    if (m_pending &&
        (m_settings.currentOwner() != m_pendingOwner ||
         m_settings.state() == ClientState::Unavailable ||
         m_settings.state() == ClientState::Degraded)) {
        markUncertain(tr("Settings changed before the per-source value could be confirmed."));
    }
    applySnapshot(false);
}

void SourceIdleDisplaySettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (!m_pending || m_waitingForReadback) return;
    if (outcome.status == SettingsWireStatus::Applied) {
        if (m_settings.currentOwner() != m_pendingOwner || !m_settings.snapshot() ||
            m_settings.snapshot()->epoch != m_pendingEpoch) {
            markUncertain(tr("Settings changed before the per-source value could be confirmed."));
            return;
        }
        m_revisionFloor = outcome.revisionAfter;
        m_waitingForReadback = true;
        m_requiredSnapshotSequence = m_snapshotSequenceAtDispatch + 1;
        m_readbackTimeout.start();
        if (m_snapshotSequence >= m_requiredSnapshotSequence)
            applySnapshot(true);
        return;
    }
    m_pending = false;
    m_errorText = outcome.message.isEmpty()
        ? Services::SettingsProtocol::settingsWireStatusName(outcome.status)
        : outcome.message.left(512);
    m_pendingKey.clear();
    m_pendingOwner.clear();
    m_pendingEpoch.clear();
    publish();
}

void SourceIdleDisplaySettingsModel::markUncertain(const QString &reason)
{
    if (!m_pending) return;
    m_pending = false;
    m_waitingForReadback = false;
    m_readbackTimeout.stop();
    m_pendingKey.clear();
    m_pendingOwner.clear();
    m_pendingEpoch.clear();
    m_errorText = reason.left(512);
    publish();
}

void SourceIdleDisplaySettingsModel::publish()
{
    if (m_pending) {
        m_statusText = m_waitingForReadback
            ? tr("Checking the saved per-source display-off preference…")
            : tr("Saving per-source display-off preference…");
    } else if (!m_available) {
        m_statusText = tr("Per-source display-off preferences are not confirmed by the current Settings1 owner.");
    } else if (m_activeSource.isEmpty()) {
        m_statusText = tr("The active power source is unknown; all confirmed source settings are shown.");
    } else {
        m_statusText = tr("%1 settings are effective; values remain available for all power sources.")
            .arg(sourceLabel(m_activeSource));
    }
    Q_EMIT changed();
}

bool SourceIdleDisplaySettingsModel::submit(const QString &entryKey,
                                             const QVariant &value)
{
    if (m_pending || !m_available || !m_settings.snapshot() ||
        !m_settings.canSetUserValue(entryKey)) {
        return false;
    }
    m_pending = true;
    m_waitingForReadback = false;
    m_pendingKey = entryKey;
    m_pendingOwner = m_settings.currentOwner();
    m_pendingEpoch = m_settings.snapshot()->epoch;
    m_requestedValue = value;
    m_snapshotSequenceAtDispatch = m_snapshotSequence;
    m_errorText.clear();
    publish();
    QString error;
    if (m_settings.setUserValue(entryKey, value, &error)) return true;
    m_pending = false;
    m_pendingKey.clear();
    m_pendingOwner.clear();
    m_pendingEpoch.clear();
    m_errorText = error.isEmpty()
        ? tr("The per-source display-off preference could not be submitted.")
        : error.left(512);
    publish();
    return false;
}

bool SourceIdleDisplaySettingsModel::setEnabled(const QString &source,
                                                 const bool enabled)
{
    if (!validSource(source) || !m_values.value(source).confirmed ||
        m_values.value(source).enabled == enabled) return false;
    return submit(key(source, QStringLiteral("displayOffEnabled")), enabled);
}

bool SourceIdleDisplaySettingsModel::setSeconds(const QString &source,
                                                 const int seconds)
{
    if (!validSource(source) || seconds < 0 || seconds > 14400 ||
        !m_values.value(source).confirmed ||
        m_values.value(source).seconds == seconds) return false;
    return submit(key(source, QStringLiteral("displayOffSeconds")), seconds);
}

bool SourceIdleDisplaySettingsModel::retry()
{
    if (m_pending) return false;
    m_settings.refresh();
    return true;
}

} // namespace QindaQt::Apps::SettingsPower
