// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/apps/settings_customize/customize_settings_model.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QMetaType>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

constexpr int ReadbackRetryMilliseconds = 200;
constexpr int ReadbackDeadlineMilliseconds = 4'000;
constexpr int MaximumDelayMilliseconds = 5'000;

bool exactDelay(const QVariant &value, int *milliseconds)
{
    if (value.metaType().id() != QMetaType::LongLong) {
        return false;
    }
    const qint64 parsed = value.toLongLong();
    if (parsed < 0 || parsed > MaximumDelayMilliseconds) {
        return false;
    }
    *milliseconds = static_cast<int>(parsed);
    return true;
}

} // namespace

bool CustomizeSettingsModel::panelHideDelayAvailable() const noexcept
{
    return m_delayHasBaseline
        && m_client.state() == Services::SettingsClient::ClientState::Ready
        && m_client.currentOwner() == m_delayOwner;
}

bool CustomizeSettingsModel::panelHideDelayEditable() const noexcept
{
    // Independent of the preset catalog: the delay stays editable even when
    // no preset can be listed. One Settings1 write at a time, so never while
    // a preset switch awaits confirmation.
    return panelHideDelayAvailable() && !busy() && !m_pendingDelay
        && m_client.canSetUserValue(QString(PanelHideDelaySettingsKey));
}

bool CustomizeSettingsModel::panelHideDelayPending() const noexcept
{
    return m_pendingDelay.has_value();
}

int CustomizeSettingsModel::panelHideDelayMs() const noexcept
{
    return m_confirmedDelayMs;
}

QString CustomizeSettingsModel::panelHideDelayStatus() const
{
    if (m_pendingDelay) {
        return m_pendingDelay->awaitingReadback
            ? QStringLiteral("Checking the saved hide delay…")
            : QStringLiteral("Saving the hide delay…");
    }
    if (!m_delayError.isEmpty()) {
        return m_delayError;
    }
    if (!panelHideDelayAvailable()) {
        return QStringLiteral("Saved hide delay is unavailable; refresh Settings to try again.");
    }
    if (busy()) {
        return QStringLiteral("Wait for the layout switch to finish.");
    }
    if (!m_client.canSetUserValue(QString(PanelHideDelaySettingsKey))) {
        return QStringLiteral("Wait for Settings to finish refreshing.");
    }
    return QStringLiteral("Saved for auto-hiding panels in every layout profile.");
}

bool CustomizeSettingsModel::setPanelHideDelayMs(const int milliseconds)
{
    if (milliseconds < 0 || milliseconds > MaximumDelayMilliseconds) {
        m_delayError = QStringLiteral("Choose a hide delay from 0 to 5000 milliseconds.");
        Q_EMIT stateChanged();
        return false;
    }
    if (!panelHideDelayEditable()) {
        m_delayError = QStringLiteral("The saved hide delay cannot be changed right now.");
        Q_EMIT stateChanged();
        return false;
    }
    if (milliseconds == m_confirmedDelayMs) {
        return true;
    }
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        m_delayError = QStringLiteral("The saved hide delay is unavailable.");
        Q_EMIT stateChanged();
        return false;
    }
    // AGENT-GUARD: Set pending before SettingsClient's synchronous admission
    // signals. The commit reply is not a confirmed value; only a later exact
    // owner/epoch snapshot at revisionAfter can retire this intent.
    m_pendingDelay = PendingDelay{milliseconds, snapshot->owner, snapshot->epoch,
                                  snapshot->revision, 0, false};
    QString error;
    if (!m_client.setUserValue(QString(PanelHideDelaySettingsKey),
                               qint64(milliseconds), &error)) {
        clearDelayPending();
        m_delayError = error.isEmpty()
            ? QStringLiteral("Settings refused the hide delay change.")
            : error.left(512);
        Q_EMIT stateChanged();
        return false;
    }
    m_delayError.clear();
    Q_EMIT stateChanged();
    return true;
}

void CustomizeSettingsModel::clearDelayPending()
{
    m_delayReadbackRetry.stop();
    m_delayReadbackDeadline.stop();
    m_pendingDelay.reset();
}

void CustomizeSettingsModel::finishDelayUncertain(const QString &message)
{
    if (!m_pendingDelay) {
        return;
    }
    clearDelayPending();
    m_delayError = message.isEmpty()
        ? QStringLiteral("The hide delay outcome is uncertain. Refresh to check its saved value.")
        : message.left(512);
    Q_EMIT stateChanged();
}

void CustomizeSettingsModel::handleDelayClientState()
{
    using Services::SettingsClient::ClientState;
    if (m_pendingDelay && m_client.currentOwner() != m_pendingDelay->owner) {
        finishDelayUncertain(QStringLiteral(
            "Settings authority changed before the hide delay was confirmed."));
    } else if (m_pendingDelay
               && (m_client.state() == ClientState::Unavailable
                   || m_client.state() == ClientState::Degraded)) {
        finishDelayUncertain(QStringLiteral(
            "The hide delay outcome is uncertain because Settings became unavailable."));
    }
    if (m_delayHasBaseline && m_client.currentOwner() != m_delayOwner) {
        m_delayHasBaseline = false;
        Q_EMIT stateChanged();
    }
}

void CustomizeSettingsModel::handleDelaySnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    if (m_pendingDelay
        && (snapshot->owner != m_pendingDelay->owner
            || snapshot->epoch != m_pendingDelay->epoch)) {
        finishDelayUncertain(QStringLiteral(
            "Settings authority changed before the hide delay was confirmed."));
    }
    int observed = 0;
    if (!exactDelay(snapshot->values.value(PanelHideDelaySettingsKey), &observed)) {
        m_delayHasBaseline = false;
        if (m_pendingDelay && m_pendingDelay->awaitingReadback) {
            finishDelayUncertain(QStringLiteral(
                "Settings returned an invalid saved hide delay."));
        }
        Q_EMIT stateChanged();
        return;
    }
    const bool externalChange = m_delayHasBaseline
        && (m_confirmedDelayMs != observed || m_delayOwner != snapshot->owner
            || m_delayEpoch != snapshot->epoch);
    m_confirmedDelayMs = observed;
    m_delayOwner = snapshot->owner;
    m_delayEpoch = snapshot->epoch;
    m_delayHasBaseline = true;

    if (m_pendingDelay && m_pendingDelay->awaitingReadback) {
        if (snapshot->revision >= m_pendingDelay->readbackFloor) {
            const bool confirmed = observed == m_pendingDelay->requestedMs;
            clearDelayPending();
            m_delayError = confirmed
                ? QString{}
                : QStringLiteral(
                    "The saved hide delay differs from the requested value.");
        }
    } else if (!m_pendingDelay && externalChange) {
        m_delayError.clear();
    }
    Q_EMIT stateChanged();
}

void CustomizeSettingsModel::handleDelayCommit(
    const Services::SettingsClient::CommitOutcome &outcome)
{
    if (!m_pendingDelay) {
        return;
    }
    if (outcome.status == Services::SettingsProtocol::SettingsWireStatus::Applied) {
        m_pendingDelay->awaitingReadback = true;
        m_pendingDelay->readbackFloor = outcome.revisionAfter;
        m_delayReadbackDeadline.start(ReadbackDeadlineMilliseconds);
        m_delayReadbackRetry.start(ReadbackRetryMilliseconds);
        Q_EMIT stateChanged();
        return;
    }
    clearDelayPending();
    m_delayError = outcome.message.isEmpty()
        ? QStringLiteral("Settings refused the hide delay change.")
        : outcome.message.left(512);
    Q_EMIT stateChanged();
}

void CustomizeSettingsModel::handleDelayUncertain(const QString &message)
{
    Q_UNUSED(message);
    finishDelayUncertain(QStringLiteral(
        "The hide delay outcome is uncertain. Refresh to check its saved value."));
    m_client.refresh();
}

} // namespace QindaQt::Apps::SettingsCustomize
