// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_applet_controller.h"

#include <qindaqt/services/brightness_model/brightness_composition.h>
#include <qindaqt/services/brightness_model/brightness_math.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/shell/power_applet/power_applet_presentation.h>

#include <QtCore/QVariantMap>

#include <algorithm>

namespace QindaQt::Shell::PowerApplet {
namespace {

QString phaseToken(const ServicePhase phase)
{
    switch (phase) {
    case ServicePhase::Loading:
        return QStringLiteral("loading");
    case ServicePhase::Ready:
        return QStringLiteral("ready");
    case ServicePhase::Degraded:
        return QStringLiteral("degraded");
    case ServicePhase::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

} // namespace

PowerAppletController::PowerAppletController(Power::PowerClient *client,
                                             const bool powerReadGranted,
                                             const bool powerControlGranted,
                                             QObject *sessionActions,
                                             QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_powerReadGranted(powerReadGranted)
    , m_powerControlGranted(powerReadGranted && powerControlGranted)
    , m_sessionActions(sessionActions)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_client->thread() == thread());
    connect(m_client, &Power::PowerClient::stateChanged, this,
            &PowerAppletController::reproject);
    connect(m_client, &Power::PowerClient::snapshotChanged, this,
            &PowerAppletController::reproject);
    connect(m_client, &Power::PowerClient::operationCompleted, this,
            &PowerAppletController::handleOperationCompleted);
    reproject();
}

QObject *PowerAppletController::sessionActions() const noexcept
{
    return m_sessionActions;
}

QString PowerAppletController::phase() const
{
    return phaseToken(m_model.phase);
}

QString PowerAppletController::batteryLabel() const
{
    if (!m_model.summary.present) {
        return m_model.phase == ServicePhase::Loading ? tr("…") : tr("Power");
    }
    if (!m_model.summary.percentageKnown) {
        return tr("Battery");
    }
    return tr("%1%").arg(qRound(m_model.summary.percentage));
}

QString PowerAppletController::accessibleName() const
{
    if (m_model.summary.present && !m_model.summary.accessibleName.isEmpty()) {
        return m_model.summary.accessibleName;
    }
    if (m_model.phase == ServicePhase::Loading) {
        return tr("Power information is loading");
    }
    return tr("Power information is unavailable");
}

QString PowerAppletController::accessibleDescription() const
{
    if (m_model.summary.present
        && !m_model.summary.accessibleDescription.isEmpty()) {
        return m_model.summary.accessibleDescription;
    }
    return m_model.diagnostic;
}

QVariantList PowerAppletController::keyboardRows() const
{
    QVariantList rows;
    rows.reserve(m_model.keyboardControls.size());
    for (const BrightnessControlRow &row : m_model.keyboardControls) {
        rows.append(QVariantMap{
            {QStringLiteral("controlId"), row.controlId},
            {QStringLiteral("name"), row.name},
            {QStringLiteral("currentKnown"), row.currentKnown},
            {QStringLiteral("normalizedCurrent"), row.normalizedCurrent},
            {QStringLiteral("adjustable"),
             m_powerControlGranted && row.adjustable},
            {QStringLiteral("pending"), operationPending()
                 && m_pendingKind == PendingKind::KeyboardBrightness
                 && m_request.device.opaqueId == row.controlId},
            {QStringLiteral("unavailableReason"), row.unavailableReason},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"),
             row.accessibleDescription},
        });
    }
    return rows;
}

QVariantList PowerAppletController::profileRows() const
{
    QVariantList rows;
    if (!presentationOwnerAvailable()) {
        return rows;
    }
    const Power::Snapshot snapshot = m_client->snapshot();
    if (!snapshot.capabilities.testFlag(Power::Capability::Profiles)) {
        return rows;
    }

    QList<Power::Profile> profiles = snapshot.profiles.supported;
    std::ranges::sort(profiles, [](const Power::Profile &left,
                                  const Power::Profile &right) {
        return left.id < right.id;
    });
    rows.reserve(profiles.size());
    for (const Power::Profile &profile : profiles) {
        const bool active = profile.id == snapshot.profiles.activeProfileId;
        const bool pending = operationPending()
            && m_pendingKind == PendingKind::Profile
            && m_pendingProfileId == profile.id;
        rows.append(QVariantMap{
            {QStringLiteral("profileId"), profile.id},
            {QStringLiteral("label"), profile.label},
            {QStringLiteral("active"), active},
            {QStringLiteral("adjustable"), m_powerControlGranted && !active},
            {QStringLiteral("pending"), pending},
            {QStringLiteral("accessibleName"),
             tr("%1 power profile").arg(profile.label)},
            {QStringLiteral("accessibleDescription"),
             active ? tr("Current power profile")
                    : m_powerControlGranted ? tr("Activate this power profile")
                                            : tr("Power profile changes are not allowed")},
        });
    }
    return rows;
}

bool PowerAppletController::operationPending() const noexcept
{
    return m_requestId != 0 && m_pendingKind != PendingKind::None;
}

bool PowerAppletController::presentationOwnerAvailable() const noexcept
{
    const auto state = m_client->state();
    return m_powerReadGranted && !m_client->owner().isEmpty()
        && m_client->hasSnapshot()
        && (state == Power::PowerClientState::Ready
            || state == Power::PowerClientState::Degraded);
}

void PowerAppletController::publishFeedback(const QString &message)
{
    if (message == m_feedback) {
        return;
    }
    m_feedback = message;
    Q_EMIT feedbackChanged();
}

void PowerAppletController::clearFeedback()
{
    publishFeedback({});
}

bool PowerAppletController::requestKeyboardBrightness(const QString &controlId,
                                                      const int normalized)
{
    if (operationPending()) {
        publishFeedback(tr("A brightness change is already in progress."));
        return false;
    }
    if (!presentationOwnerAvailable()) {
        publishFeedback(tr("Power information is unavailable, so no change was sent."));
        return false;
    }
    if (!m_powerControlGranted) {
        publishFeedback(tr("Power controls are not allowed for this applet."));
        return false;
    }
    if (normalized < 0
        || normalized > static_cast<int>(Power::kNormalizedBrightnessMaximum)) {
        publishFeedback(tr("That brightness value was not accepted."));
        return false;
    }

    const Power::Snapshot snapshot = m_client->snapshot();
    const Power::KeyboardBacklight *device = nullptr;
    for (const Power::KeyboardBacklight &candidate : snapshot.keyboardBacklights) {
        if (candidate.handle.opaqueId == controlId) {
            device = &candidate;
            break;
        }
    }
    if (device == nullptr) {
        publishFeedback(tr("That keyboard backlight is no longer listed."));
        return false;
    }

    const BrightnessRequest request =
        beginKeyboardBrightnessRequest(snapshot, device->handle);
    if (request.phase != RequestPhase::Pending) {
        publishFeedback(request.feedback);
        return false;
    }
    const Brightness::RawResult raw = Brightness::denormalizeRaw(
        0, device->maximum, static_cast<quint32>(normalized));
    if (!raw.succeeded()) {
        publishFeedback(tr("That keyboard backlight range is not usable."));
        return false;
    }

    const quint64 requestId =
        m_client->setKeyboardBrightness(device->handle, raw.value);
    if (requestId == 0) {
        publishFeedback(tr("The brightness change could not be sent."));
        return false;
    }
    m_request = request;
    m_requestId = requestId;
    m_pendingKind = PendingKind::KeyboardBrightness;
    m_pendingEpoch = request.initiatingEpoch;
    m_pendingRevision = request.initiatingRevision;
    publishFeedback({});
    Q_EMIT stateChanged();
    return true;
}

bool PowerAppletController::requestProfile(const QString &profileId)
{
    if (operationPending()) {
        publishFeedback(tr("A power change is already in progress."));
        return false;
    }
    if (!presentationOwnerAvailable()) {
        publishFeedback(tr("Power information is unavailable, so no change was sent."));
        return false;
    }
    if (!m_powerControlGranted) {
        publishFeedback(tr("Power controls are not allowed for this applet."));
        return false;
    }

    const Power::Snapshot snapshot = m_client->snapshot();
    if (!snapshot.capabilities.testFlag(Power::Capability::Profiles)) {
        publishFeedback(tr("Power profiles are not available."));
        return false;
    }
    const auto profile = std::ranges::find_if(
        snapshot.profiles.supported,
        [&profileId](const Power::Profile &candidate) {
            return candidate.id == profileId;
        });
    if (profile == snapshot.profiles.supported.cend()) {
        publishFeedback(tr("That power profile is no longer listed."));
        return false;
    }
    if (profileId == snapshot.profiles.activeProfileId) {
        publishFeedback(tr("That power profile is already active."));
        return false;
    }

    const quint64 requestId = m_client->setProfile(profileId);
    if (requestId == 0) {
        publishFeedback(tr("The power profile change could not be sent."));
        return false;
    }
    m_requestId = requestId;
    m_pendingKind = PendingKind::Profile;
    m_pendingProfileId = profileId;
    m_pendingEpoch = snapshot.epoch;
    m_pendingRevision = snapshot.revision;
    publishFeedback({});
    Q_EMIT stateChanged();
    return true;
}

QString PowerAppletController::profileResultFeedback(
    const Power::OperationResult &result) const
{
    const bool validVocabulary = static_cast<quint32>(result.status)
        <= static_cast<quint32>(Power::OperationStatus::Inhibited);
    const bool exactInitiator = result.wireValid
        && result.kind == Power::OperationKind::SetProfile
        && result.initiatingEpoch == m_pendingEpoch
        && result.initiatingRevision == m_pendingRevision;
    if (!validVocabulary || !exactInitiator) {
        return tr("The power service returned an unreadable result. Check the current profile before retrying.");
    }
    if (result.status == Power::OperationStatus::Succeeded) {
        if (result.observedEpoch == m_pendingEpoch
            && result.observedRevision >= m_pendingRevision) {
            return {};
        }
        return tr("The power profile may have changed. Check the current profile before retrying.");
    }
    switch (result.status) {
    case Power::OperationStatus::Rejected:
        return tr("The power profile change was rejected.");
    case Power::OperationStatus::Unsupported:
        return tr("Power profile changes are not supported.");
    case Power::OperationStatus::Busy:
        return tr("The power service is busy; try again later.");
    case Power::OperationStatus::AuthenticationRequired:
        return tr("Authentication is required to change the power profile.");
    case Power::OperationStatus::Inhibited:
        return tr("The power profile change is inhibited right now.");
    case Power::OperationStatus::Failed:
        return tr("The power profile change failed.");
    case Power::OperationStatus::Uncertain:
        return tr("The power profile may have changed. Check the current profile before retrying.");
    case Power::OperationStatus::Succeeded:
        break;
    }
    return tr("The power profile result could not be confirmed.");
}

void PowerAppletController::handleOperationCompleted(
    const quint64 requestId, const Power::OperationResult &result)
{
    if (requestId != m_requestId || m_requestId == 0) {
        return;
    }
    if (m_pendingKind == PendingKind::KeyboardBrightness) {
        const BrightnessRequest updated = applyOperationResult(m_request, result);
        if (updated.phase == RequestPhase::Pending) {
            return;
        }
        m_request = updated;
        publishFeedback(updated.phase == RequestPhase::Succeeded
                            ? QString{}
                            : updated.feedback);
    } else if (m_pendingKind == PendingKind::Profile) {
        publishFeedback(profileResultFeedback(result));
    }
    m_requestId = 0;
    m_pendingKind = PendingKind::None;
    m_pendingProfileId.clear();
    m_pendingEpoch = 0;
    m_pendingRevision = 0;
    Q_EMIT stateChanged();
}

void PowerAppletController::reproject()
{
    const bool exactOwnerAvailable =
        !m_client->owner().isEmpty() && m_client->hasSnapshot();
    if (operationPending()) {
        const quint64 epoch = m_client->hasSnapshot()
            ? m_client->snapshot().epoch
            : 0;
        if (m_pendingKind == PendingKind::KeyboardBrightness) {
            const BrightnessRequest observed =
                observeGeneration(m_request, exactOwnerAvailable, epoch);
            if (observed.phase != RequestPhase::Pending) {
                m_request = observed;
                m_requestId = 0;
                m_pendingKind = PendingKind::None;
                m_pendingEpoch = 0;
                m_pendingRevision = 0;
                publishFeedback(m_request.feedback);
            }
        } else if (!exactOwnerAvailable || epoch != m_pendingEpoch) {
            m_requestId = 0;
            m_pendingKind = PendingKind::None;
            m_pendingProfileId.clear();
            m_pendingEpoch = 0;
            m_pendingRevision = 0;
            publishFeedback(tr("The power service was replaced. Check the current profile before retrying."));
        }
    }

    if (!presentationOwnerAvailable()) {
        m_model = {};
        if (!m_powerReadGranted) {
            m_model.phase = ServicePhase::Unavailable;
            m_model.diagnostic = tr("Power access was not granted to this applet.");
        } else if (m_client->state() == Power::PowerClientState::Stopped
            || m_client->state() == Power::PowerClientState::Starting) {
            m_model.phase = ServicePhase::Loading;
            m_model.diagnostic = tr("Power information is loading.");
        } else {
            m_model.phase = ServicePhase::Unavailable;
            m_model.diagnostic = tr("Power information is unavailable.");
        }
        Q_EMIT stateChanged();
        return;
    }

    const Power::Snapshot snapshot = m_client->snapshot();
    const Brightness::CompositionResult composition =
        Brightness::composeBrightness({}, {.ownerAvailable = true,
                                           .snapshot = snapshot});
    BrightnessView brightness;
    if (composition.succeeded()) {
        brightness.ownerAvailable = true;
        brightness.model = composition.snapshot;
    }
    m_model = projectPowerApplet(snapshot, true, brightness);
    Q_EMIT stateChanged();
}

} // namespace QindaQt::Shell::PowerApplet
