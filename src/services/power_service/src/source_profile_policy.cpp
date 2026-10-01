// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/source_profile_policy.h>
#include <QtCore/QUuid>

namespace QindaQt::Power {
namespace {
const QString applicationId = QStringLiteral("org.qindaqt.Power1.SourcePolicy");
}
QStringList SourceProfilePolicy::settingsKeys()
{
    return {QStringLiteral("power.profile.ac"), QStringLiteral("power.profile.battery"),
            QStringLiteral("power.profile.lowBattery")};
}
SourceProfilePolicy::SourceProfilePolicy(
    PowerServiceCoordinator &power, Services::SettingsClient::SettingsClient &settings,
    const int timeout, QObject *parent)
    : QObject(parent), m_power(power), m_settings(settings)
{
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(qMax(1, timeout));
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        if (m_kind == OperationKind::ReleaseProfileHold && m_owned.isValid())
            m_uncertainReleases.insert(m_owned.opaqueId);
        m_operation = 0;
        m_awaitingObservation = false;
        m_quarantined = true;
        schedule();
    });
    connect(&power, &PowerServiceCoordinator::profileHoldCancelled, this,
            [this](const Handle &handle) {
                if (handle != m_owned || !handle.isValid()) return;
                m_manualSuppressed = selectionKey();
                schedule();
            });
    connect(&power, &PowerServiceCoordinator::snapshotChanged, this,
            [this] { schedule(); });
    connect(&power, &PowerServiceCoordinator::operationCompleted, this,
            [this](quint64 id, const OperationResult &result) {
                // A collaborator may finish inside submit(). Defer delivery
                // until the returned operation ID has been recorded.
                QTimer::singleShot(0, this, [this, id, result] { completed(id, result); });
            });
    connect(&settings, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &SourceProfilePolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::ownerChanged,
            this, &SourceProfilePolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &SourceProfilePolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::writeAdmissionChanged,
            this, &SourceProfilePolicy::schedule);
}
void SourceProfilePolicy::setNativeAuthority(const bool admitted)
{
    if (m_authority == admitted) return;
    m_authority = admitted;
    schedule();
}
void SourceProfilePolicy::retry()
{
    m_failedKey.clear();
    m_manualSuppressed.clear();
    schedule();
}
void SourceProfilePolicy::schedule()
{
    if (m_scheduled) return;
    m_scheduled = true;
    QTimer::singleShot(0, this, [this] { m_scheduled = false; reconcile(); });
}
QString SourceProfilePolicy::desiredProfile() const
{
    using Services::SettingsClient::ClientState;
    const auto &settings = m_settings.snapshot();
    const auto &power = m_power.snapshot();
    if (!m_authority || m_settings.state() != ClientState::Ready || !settings
        || settings->owner != m_settings.currentOwner()
        || !power.capabilities.testFlag(Capability::Supplies)
        || !power.capabilities.testFlag(Capability::ProfileHolds)) return {};
    QString source;
    if (power.source.onBattery) {
        const auto warning = power.composite.warning;
        source = warning == WarningLevel::Low || warning == WarningLevel::Critical
                || warning == WarningLevel::Action
            ? QStringLiteral("lowBattery") : QStringLiteral("battery");
    } else if (power.source.acPresent) {
        source = QStringLiteral("ac");
    } else {
        return {};
    }
    const QVariant value = settings->values.value(QStringLiteral("power.profile.") + source);
    if (value.metaType() != QMetaType::fromType<QString>()) return {};
    const QString desired = value.toString();
    if (desired != QStringLiteral("power-saver")
        && desired != QStringLiteral("performance")) return {};
    for (const auto &profile : power.profiles.supported)
        if (profile.id == desired) return desired;
    return {};
}
QString SourceProfilePolicy::selectionKey() const
{
    const auto &snapshot = m_power.snapshot();
    const auto &settings = m_settings.snapshot();
    QString source = QStringLiteral("unknown");
    if (snapshot.capabilities.testFlag(Capability::Supplies)) {
        if (snapshot.source.onBattery) {
            const auto warning = snapshot.composite.warning;
            source = warning == WarningLevel::Low || warning == WarningLevel::Critical
                    || warning == WarningLevel::Action
                ? QStringLiteral("lowBattery") : QStringLiteral("battery");
        } else if (snapshot.source.acPresent) source = QStringLiteral("ac");
    }
    return QString::number(m_authority) + QLatin1Char('|') + source + QLatin1Char('|')
        + (settings ? settings->owner + settings->epoch + QLatin1Char('|')
                       + settings->values.value(QStringLiteral("power.profile.") + source).toString()
                    : QString());
}
void SourceProfilePolicy::reconcile()
{
    const auto &snapshot = m_power.snapshot();
    if (!m_manualSuppressed.isEmpty() && m_manualSuppressed != selectionKey())
        m_manualSuppressed.clear();
    if (m_epoch != 0 && snapshot.epoch != m_epoch) {
        if (m_operation != 0 || m_awaitingObservation) {
            m_quarantined = true;
            if (m_kind == OperationKind::ReleaseProfileHold && m_owned.isValid())
                m_uncertainReleases.insert(m_owned.opaqueId);
        }
        m_operation = 0;
        m_awaitingObservation = false;
        m_deadline.stop();
        m_owned = {};
        m_failedKey.clear();
    }
    m_epoch = snapshot.epoch;
    const ProfileHold *observed = nullptr;
    for (const auto &hold : snapshot.profiles.holds) {
        if (!m_reason.isEmpty() && hold.applicationName == applicationId
            && hold.reason == m_reason && hold.profileId == m_requestedProfile) {
            observed = &hold;
            break;
        }
    }
    if (!observed && m_owned.isValid() && m_operation == 0 && !m_awaitingObservation) {
        // A user changing the provider's base profile cancels its holds.
        // Equivalent observations must not fight that manual override.
        m_manualSuppressed = selectionKey();
    }
    m_owned = observed ? observed->handle : Handle{};
    if (m_operation != 0) return;
    if (m_awaitingObservation) {
        const bool converged = m_kind == OperationKind::AcquireProfileHold
            ? observed != nullptr : observed == nullptr;
        if (!converged) return;
        m_awaitingObservation = false;
        m_deadline.stop();
        if (m_kind == OperationKind::ReleaseProfileHold) {
            m_reason.clear();
            m_requestedProfile.clear();
        }
    }
    const QString desired = desiredProfile();
    const auto &settings = m_settings.snapshot();
    QString admission;
    for (const auto &profile : snapshot.profiles.supported) admission += profile.id + QLatin1Char(',');
    for (const auto &hold : snapshot.profiles.holds) admission += hold.handle.opaqueId + QLatin1Char(',');
    const QString key = admission + QString::number(snapshot.epoch) + QLatin1Char('|') + desired
        + QLatin1Char('|') + (settings ? settings->owner + settings->epoch
                                               : QString());
    if (observed) {
        if (desired == m_requestedProfile && !m_quarantined) return;
        // Keep the already-confirmed hold while a refresh is in flight, but
        // never select a different source from its retained preference map.
        if (!desired.isEmpty() && !m_quarantined
            && !m_settings.canSetUserValue(QStringLiteral("power.profile.ac"))) return;
        if (m_failedKey == key || m_uncertainReleases.contains(m_owned.opaqueId)) return;
        m_attemptKey = key;
        PowerServiceRequest request;
        request.kind = OperationKind::ReleaseProfileHold;
        request.handle = m_owned;
        dispatch(request);
        return;
    }
    if (m_quarantined || desired.isEmpty() || m_failedKey == key
        || m_manualSuppressed == selectionKey()
        || !m_settings.canSetUserValue(QStringLiteral("power.profile.ac"))) return;
    m_reason = QStringLiteral("Automatic source profile %1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
    m_requestedProfile = desired;
    m_attemptKey = key;
    PowerServiceRequest request;
    request.kind = OperationKind::AcquireProfileHold;
    request.profileId = desired;
    request.applicationName = applicationId;
    request.reason = m_reason;
    dispatch(request);
}
void SourceProfilePolicy::dispatch(PowerServiceRequest request)
{
    m_kind = request.kind;
    const auto submission = m_power.submit(request);
    if (!submission.pending) {
        m_failedKey = m_attemptKey;
        return;
    }
    m_operation = submission.operationId;
    m_deadline.start();
}
void SourceProfilePolicy::completed(const quint64 id, const OperationResult &result)
{
    if (id != m_operation || id == 0) return;
    m_operation = 0;
    if (result.status == OperationStatus::Succeeded) {
        m_awaitingObservation = true;
    } else {
        m_deadline.stop();
        m_failedKey = m_attemptKey;
        if (result.status == OperationStatus::Uncertain) {
            m_quarantined = true;
            if (m_kind == OperationKind::ReleaseProfileHold && m_owned.isValid())
                m_uncertainReleases.insert(m_owned.opaqueId);
        }
    }
    schedule();
}
} // namespace QindaQt::Power
