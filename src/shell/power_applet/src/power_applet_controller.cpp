// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_applet_controller.h"

#include <qindaqt/services/brightness_model/brightness_composition.h>
#include <qindaqt/services/brightness_model/brightness_math.h>
#include <qindaqt/shell/power_applet/power_applet_presentation.h>

#include <QtCore/QVariantMap>

#include <cmath>

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
                                             QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    connect(m_client, &Power::PowerClient::stateChanged, this,
            &PowerAppletController::reproject);
    connect(m_client, &Power::PowerClient::snapshotChanged, this,
            &PowerAppletController::reproject);
    connect(m_client, &Power::PowerClient::operationCompleted, this,
            &PowerAppletController::handleOperationCompleted);
    reproject();
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
            {QStringLiteral("adjustable"), row.adjustable},
            {QStringLiteral("pending"), operationPending()
                 && m_request.device.opaqueId == row.controlId},
            {QStringLiteral("unavailableReason"), row.unavailableReason},
            {QStringLiteral("accessibleName"), row.accessibleName},
            {QStringLiteral("accessibleDescription"),
             row.accessibleDescription},
        });
    }
    return rows;
}

bool PowerAppletController::operationPending() const noexcept
{
    return m_requestId != 0 && m_request.phase == RequestPhase::Pending;
}

bool PowerAppletController::presentationOwnerAvailable() const noexcept
{
    const auto state = m_client->state();
    return !m_client->owner().isEmpty() && m_client->hasSnapshot()
        && (state == Power::PowerClientState::Ready
            || state == Power::PowerClientState::Degraded);
}

const Power::KeyboardBacklight *
PowerAppletController::currentKeyboard(const QString &controlId) const
{
    if (!presentationOwnerAvailable()) {
        return nullptr;
    }
    // The returned address points into this temporary snapshot, so callers
    // must not use this helper. Kept unreachable to prevent accidental use.
    Q_UNREACHABLE_RETURN(nullptr);
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
    publishFeedback({});
    Q_EMIT stateChanged();
    return true;
}

void PowerAppletController::handleOperationCompleted(
    const quint64 requestId, const Power::OperationResult &result)
{
    if (requestId != m_requestId || m_requestId == 0) {
        return;
    }
    m_request = applyOperationResult(m_request, result);
    m_requestId = 0;
    if (!m_request.feedback.isEmpty()) {
        publishFeedback(m_request.feedback);
    }
    Q_EMIT stateChanged();
}

void PowerAppletController::reproject()
{
    const bool exactOwnerAvailable =
        !m_client->owner().isEmpty() && m_client->hasSnapshot();
    if (m_request.phase == RequestPhase::Pending) {
        const quint64 epoch = m_client->hasSnapshot()
            ? m_client->snapshot().epoch
            : 0;
        const BrightnessRequest observed =
            observeGeneration(m_request, exactOwnerAvailable, epoch);
        if (observed.phase != RequestPhase::Pending) {
            m_request = observed;
            m_requestId = 0;
            publishFeedback(m_request.feedback);
        }
    }

    if (!presentationOwnerAvailable()) {
        m_model = {};
        if (m_client->state() == Power::PowerClientState::Stopped
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
