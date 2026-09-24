// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_settings_model.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QMetaType>

#include <utility>

namespace QindaQt::Apps::SettingsCustomize {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {

constexpr int SelectionReadbackDeadlineMilliseconds = 4'000;
constexpr int StoreReloadDebounceMilliseconds = 250;

} // namespace

CustomizeSettingsModel::CustomizeSettingsModel(
    Services::SettingsClient::SettingsClient &client,
    PresetLocations locations,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_locations(std::move(locations))
    , m_store(m_locations.userDirectory)
{
    Q_ASSERT(m_client.thread() == thread());
    m_delayReadbackRetry.setInterval(200);
    connect(&m_delayReadbackRetry, &QTimer::timeout, this, [this] {
        if (m_pendingDelay && m_pendingDelay->awaitingReadback) {
            m_client.refresh();
        }
    });
    m_delayReadbackDeadline.setSingleShot(true);
    connect(&m_delayReadbackDeadline, &QTimer::timeout, this, [this] {
        finishDelayUncertain(QStringLiteral(
            "The saved hide delay could not be confirmed. Refresh to check its value."));
        m_client.refresh();
    });
    m_selectionReadbackDeadline.setSingleShot(true);
    m_selectionReadbackDeadline.setInterval(SelectionReadbackDeadlineMilliseconds);
    connect(&m_selectionReadbackDeadline, &QTimer::timeout, this, [this] {
        if (m_pendingSelection && m_pendingSelection->awaitingReadback) {
            settleSelection(false, QStringLiteral(
                "Settings accepted the layout change, but it could not be confirmed. "
                "Refresh to check which layout is selected."));
            m_client.refresh();
        }
    });
    // Panel edits land in the user store while this page is open; the
    // debounce folds the shell's write bursts into one reload.
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(StoreReloadDebounceMilliseconds);
    connect(&m_reloadDebounce, &QTimer::timeout, this, &CustomizeSettingsModel::reloadPresets);
    connect(&m_storeWatch, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_reloadDebounce.start(); });
    connect(&m_storeWatch, &QFileSystemWatcher::fileChanged, this,
            [this] { m_reloadDebounce.start(); });

    connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &CustomizeSettingsModel::handleClientState);
    connect(&m_client, &Services::SettingsClient::SettingsClient::ownerChanged,
            this, &CustomizeSettingsModel::handleClientState);
    connect(&m_client, &Services::SettingsClient::SettingsClient::writeAdmissionChanged,
            this, &CustomizeSettingsModel::stateChanged);
    connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &CustomizeSettingsModel::handleSnapshot);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
            this, &CustomizeSettingsModel::handleCommit);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
            this, &CustomizeSettingsModel::handleUncertain);
    reloadPresets();
}

bool CustomizeSettingsModel::canSwitch() const noexcept
{
    return ready() && !busy() && !m_pendingDelay
        && m_client.canSetUserValue(QString(LayoutProfileSettingsKey));
}

bool CustomizeSettingsModel::canManage() const noexcept
{
    return ready() && !busy();
}

QString CustomizeSettingsModel::statusText() const
{
    switch (m_state) {
    case State::Loading:
        return QStringLiteral("Loading layout presets…");
    case State::Unavailable:
        return m_stateReason.isEmpty() ? QStringLiteral("Layout presets are unavailable")
                                       : m_stateReason;
    case State::Ready:
        break;
    }
    if (m_pendingSelection) {
        return QStringLiteral("Switching to %1…")
            .arg(presetName(m_pendingSelection->requestedId));
    }
    if (findPreset(m_activeId) == nullptr) {
        // The shell falls back to the default layout for a missing selection
        // (ADR-0263); say so instead of pretending a card is active.
        return QStringLiteral(
                   "The selected layout “%1” is not installed, so the desktop uses "
                   "the default. Choose a preset.")
            .arg(m_activeId);
    }
    return QStringLiteral("Current layout: %1").arg(presetName(m_activeId));
}

void CustomizeSettingsModel::updateState()
{
    State next = State::Ready;
    QString reason;
    if (!m_catalogError.isEmpty()) {
        next = State::Unavailable;
        reason = m_catalogError;
    } else if (m_client.state() == ClientState::Unavailable
               || m_client.state() == ClientState::Degraded) {
        next = State::Unavailable;
        reason = m_client.lastError().isEmpty()
            ? QStringLiteral("Settings1 transport is unavailable")
            : m_client.lastError();
    } else if (!m_selectionError.isEmpty()) {
        next = State::Unavailable;
        reason = m_selectionError;
    } else if (!m_hasSelection) {
        // AGENT-NOTE: after a confirmed baseline the page stays Ready while
        // the client re-authenticates after each commit; canSwitch() is what
        // follows the client's write admission.
        next = State::Loading;
    }
    m_state = next;
    m_stateReason = reason.left(512);
    Q_EMIT stateChanged();
}

void CustomizeSettingsModel::report(QString notice, QString error)
{
    m_notice = std::move(notice).left(512);
    m_error = std::move(error).left(512);
    Q_EMIT stateChanged();
}

void CustomizeSettingsModel::handleClientState()
{
    handleDelayClientState();
    // AGENT-NOTE: a commit timeout reaches here first: SettingsClient
    // publishes Degraded before it emits commitUncertain, so this is where
    // an unanswered switch becomes uncertain. Nothing is ever replayed.
    if (m_pendingSelection && m_client.currentOwner() != m_pendingSelection->owner) {
        settleSelection(false, QStringLiteral(
            "Settings restarted before the layout change was confirmed, so its outcome "
            "is uncertain. Refresh to check which layout is selected."));
    } else if (m_pendingSelection
               && (m_client.state() == ClientState::Unavailable
                   || m_client.state() == ClientState::Degraded)) {
        settleSelection(false, QStringLiteral(
            "The layout change is uncertain because Settings became unavailable. "
            "Refresh to check which layout is selected."));
    }
    updateState();
}

void CustomizeSettingsModel::handleSnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    handleDelaySnapshot();
    const QVariant selected = snapshot->values.value(LayoutProfileSettingsKey);
    if (selected.metaType().id() != QMetaType::QString
        || selected.toString().trimmed().isEmpty()) {
        m_hasSelection = false;
        m_selectionError = QStringLiteral(
            "Settings1 returned an invalid panels.layoutProfile value");
        if (m_pendingSelection) {
            settleSelection(false, m_selectionError);
        }
        updateState();
        Q_EMIT presetsChanged();
        return;
    }
    m_selectionError.clear();
    m_activeId = selected.toString();
    m_hasSelection = true;
    if (m_pendingSelection) {
        if (snapshot->owner != m_pendingSelection->owner
            || snapshot->epoch != m_pendingSelection->epoch) {
            settleSelection(false, QStringLiteral(
                "Settings restarted before the layout change was confirmed, so its "
                "outcome is uncertain. Refresh to check which layout is selected."));
        } else if (m_pendingSelection->awaitingReadback
                   && snapshot->revision >= m_pendingSelection->readbackFloor) {
            settleSelection(m_activeId == m_pendingSelection->requestedId, {});
        }
        // A same-lineage snapshot older than the Applied revision is stale:
        // keep waiting for a newer one or the bounded deadline.
    }
    updateState();
    Q_EMIT presetsChanged();
}

bool CustomizeSettingsModel::activatePreset(const QString &presetId)
{
    if (presetId == m_activeId && !busy()) {
        return true;
    }
    if (!canSwitch()) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    if (findPreset(presetId) == nullptr) {
        report({}, QStringLiteral("That preset is no longer available."));
        return false;
    }
    report({}, {});
    return beginSelection(presetId, {});
}

bool CustomizeSettingsModel::beginSelection(const QString &presetId,
                                            const QString &deleteAfter)
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    // AGENT-GUARD: set the pending intent before SettingsClient's synchronous
    // admission signals. The commit reply is not the switch: only a later
    // same owner/epoch snapshot at revisionAfter confirms it.
    m_pendingSelection = PendingSelection{presetId, deleteAfter, snapshot->owner,
                                          snapshot->epoch, 0, false};
    Q_EMIT stateChanged();
    QString error;
    if (!m_client.setUserValue(QString(LayoutProfileSettingsKey), presetId, &error)) {
        m_pendingSelection.reset();
        report({}, error.isEmpty() ? QStringLiteral("Settings refused the layout change.")
                                   : error);
        return false;
    }
    return true;
}

void CustomizeSettingsModel::settleSelection(bool confirmed, const QString &message)
{
    if (!m_pendingSelection) {
        return;
    }
    m_selectionReadbackDeadline.stop();
    const PendingSelection pending = *m_pendingSelection;
    m_pendingSelection.reset();
    if (!confirmed) {
        QString error = message.isEmpty()
            ? QStringLiteral("The layout selection changed elsewhere before it could be confirmed.")
            : message;
        if (!pending.deleteAfter.isEmpty()) {
            error += QStringLiteral(" “%1” was not deleted.").arg(presetName(pending.deleteAfter));
        }
        report({}, error);
        return;
    }
    if (!pending.deleteAfter.isEmpty()) {
        // The desktop has left the preset, so its file can go now.
        const QString name = presetName(pending.deleteAfter);
        const bool removed = removeUserCopy(
            pending.deleteAfter,
            QStringLiteral("Switched to %1 and deleted “%2”.")
                .arg(presetName(pending.requestedId), name));
        Q_UNUSED(removed);
        return;
    }
    report(QStringLiteral("Switched to %1.").arg(presetName(pending.requestedId)));
}

void CustomizeSettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (m_pendingDelay) {
        handleDelayCommit(outcome);
        return;
    }
    if (!m_pendingSelection) {
        return;
    }
    if (outcome.status == SettingsWireStatus::Applied) {
        m_pendingSelection->awaitingReadback = true;
        m_pendingSelection->readbackFloor = outcome.revisionAfter;
        m_selectionReadbackDeadline.start();
        Q_EMIT stateChanged();
        return;
    }
    const QString reason = outcome.message.isEmpty()
        ? Services::SettingsProtocol::settingsWireStatusName(outcome.status)
        : outcome.message.left(512);
    const bool conflict = outcome.status == SettingsWireStatus::Conflict
        || outcome.status == SettingsWireStatus::EpochMismatch;
    settleSelection(false, conflict
                               ? QStringLiteral("The layout selection changed elsewhere: %1").arg(reason)
                               : QStringLiteral("Settings refused the layout change: %1").arg(reason));
}

void CustomizeSettingsModel::handleUncertain(const QString &message)
{
    if (m_pendingDelay) {
        handleDelayUncertain(message);
        return;
    }
    if (!m_pendingSelection) {
        return;
    }
    settleSelection(false, QStringLiteral(
        "The layout change is uncertain. Refresh to check which layout is selected; "
        "nothing was retried."));
    m_client.refresh();
}

void CustomizeSettingsModel::retry()
{
    report({}, {});
    reloadPresets();
    m_client.refresh();
}

} // namespace QindaQt::Apps::SettingsCustomize
