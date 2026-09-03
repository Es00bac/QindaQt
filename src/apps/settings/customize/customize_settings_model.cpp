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

CustomizeSettingsModel::CustomizeSettingsModel(
    Services::SettingsClient::SettingsClient &client,
    QVector<Profiles::LayoutProfile> availableProfiles,
    QVector<Applets::AppletManifest> manifests,
    EditorHostFactory hostFactory,
    QString startupError,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_profiles(std::move(availableProfiles))
    , m_manifests(std::move(manifests))
    , m_hostFactory(std::move(hostFactory))
    , m_startupError(std::move(startupError))
{
    Q_ASSERT(m_client.thread() == thread());
    connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &CustomizeSettingsModel::handleClientState);
    connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &CustomizeSettingsModel::handleSnapshot);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
            this, &CustomizeSettingsModel::handleCommit);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
            this, &CustomizeSettingsModel::handleUncertain);

    if (!m_startupError.isEmpty() || m_profiles.isEmpty()
        || m_manifests.isEmpty() || !m_hostFactory) {
        setState(State::Unavailable,
                 !m_startupError.isEmpty()
                     ? m_startupError
                     : QStringLiteral("Customize catalogs are unavailable"));
    }
}

bool CustomizeSettingsModel::loading() const noexcept
{
    return m_state == State::Loading;
}

bool CustomizeSettingsModel::ready() const noexcept
{
    return m_state == State::Ready;
}

bool CustomizeSettingsModel::saving() const noexcept
{
    return m_state == State::Saving;
}

bool CustomizeSettingsModel::conflict() const noexcept
{
    return m_state == State::Conflict;
}

bool CustomizeSettingsModel::unavailable() const noexcept
{
    return m_state == State::Unavailable;
}

bool CustomizeSettingsModel::canEdit() const noexcept
{
    return (ready() || conflict()) && m_editor && m_editor->ready()
        && m_client.state() == ClientState::Ready;
}

bool CustomizeSettingsModel::dirty() const noexcept
{
    return m_selectionDirty || (m_editor && m_editor->dirty());
}

bool CustomizeSettingsModel::applyAvailable() const noexcept
{
    return canEdit() && dirty() && !visualDragActive();
}

bool CustomizeSettingsModel::canUndo() const noexcept
{
    return canEdit() && m_editor->canUndo();
}

bool CustomizeSettingsModel::canRedo() const noexcept
{
    return canEdit() && m_editor->canRedo();
}

bool CustomizeSettingsModel::visualDragActive() const noexcept
{
    return m_editor && m_editor->visualDragActive();
}

bool CustomizeSettingsModel::dropAccepted() const noexcept
{
    return m_lastDropAccepted;
}

QString CustomizeSettingsModel::dropReason() const
{
    return m_lastDropReason;
}

QString CustomizeSettingsModel::statusText() const
{
    if (loading()) {
        return QStringLiteral("Loading the selected layout profile…");
    }
    if (saving()) {
        return QStringLiteral("Applying the profile and selection…");
    }
    if (conflict()) {
        return QStringLiteral(
            "The selected profile changed elsewhere; review this draft before applying");
    }
    if (unavailable()) {
        return QStringLiteral("Customize is unavailable");
    }
    if (dirty()) {
        return QStringLiteral("Unapplied layout changes");
    }
    return QStringLiteral(
        "Applied profiles are adopted by qindaqt-shell at its next start");
}

QString CustomizeSettingsModel::errorText() const
{
    return m_error;
}

void CustomizeSettingsModel::setState(State state, QString error)
{
    const bool changed = m_state != state || m_error != error;
    m_state = state;
    m_error = std::move(error).left(512);
    if (changed) {
        Q_EMIT stateChanged();
    }
}

const Profiles::LayoutProfile *CustomizeSettingsModel::findProfile(
    const QString &id) const
{
    for (const auto &profile : m_profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

const Applets::AppletManifest *CustomizeSettingsModel::findManifest(
    const QString &id) const
{
    for (const auto &manifest : m_manifests) {
        if (manifest.id == id) {
            return &manifest;
        }
    }
    return nullptr;
}

bool CustomizeSettingsModel::rebuild(const Profiles::LayoutProfile &profile)
{
    std::unique_ptr<CustomizeEditorHost> next = m_hostFactory(profile);
    if (!next) {
        m_editorUnavailable = true;
        setState(State::Unavailable,
                 QStringLiteral("The layout editor repository could not be created"));
        return false;
    }
    if (!next->ready()) {
        const QString reason = next->unavailableReason();
        m_editor = std::move(next);
        m_editorUnavailable = true;
        setState(State::Unavailable,
                 reason.isEmpty() ? QStringLiteral("The editor lease is unavailable")
                                  : reason);
        Q_EMIT contentChanged();
        return false;
    }
    m_editor = std::move(next);
    m_editorUnavailable = false;
    m_selectedProfileId = profile.id;
    m_keyboardMoving = false;
    m_lastDropAccepted = false;
    m_lastDropReason.clear();
    clearSelectionIfMissing();
    Q_EMIT contentChanged();
    return true;
}

void CustomizeSettingsModel::handleClientState()
{
    if (!m_startupError.isEmpty()) {
        setState(State::Unavailable, m_startupError);
        return;
    }
    switch (m_client.state()) {
    case ClientState::Ready:
        break;
    case ClientState::Authenticating:
        if (!m_waitingForCommitSnapshot && !conflict()) {
            setState(m_hasBaseline ? State::Unavailable : State::Loading,
                     m_client.lastError());
        }
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
        m_waitingForCommitSnapshot = false;
        setState(State::Unavailable,
                 m_client.lastError().isEmpty()
                     ? QStringLiteral("Settings1 transport is unavailable")
                     : m_client.lastError());
        break;
    }
}

void CustomizeSettingsModel::handleSnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    const QVariant selected = snapshot->values.value(LayoutProfileSettingsKey);
    if (selected.metaType().id() != QMetaType::QString
        || selected.toString().trimmed().isEmpty()) {
        setState(State::Unavailable,
                 QStringLiteral("Settings1 returned an invalid panels.layoutProfile value"));
        return;
    }
    const QString authoritativeId = selected.toString();
    const Profiles::LayoutProfile *authoritative = findProfile(authoritativeId);
    if (authoritative == nullptr) {
        setState(State::Unavailable,
                 QStringLiteral("Selected profile '%1' is not installed")
                     .arg(authoritativeId));
        return;
    }

    const bool lineageChanged = m_hasBaseline
        && (m_confirmedOwner != snapshot->owner
            || m_confirmedEpoch != snapshot->epoch);
    m_confirmedOwner = snapshot->owner;
    m_confirmedEpoch = snapshot->epoch;
    m_confirmedProfileId = authoritativeId;

    if (m_hasBaseline && m_editorUnavailable) {
        m_selectionDirty = false;
        if (rebuild(*authoritative)) {
            setState(State::Ready);
        }
        return;
    }

    if (!m_hasBaseline) {
        m_hasBaseline = true;
        m_selectionDirty = false;
        if (rebuild(*authoritative)) {
            setState(State::Ready);
        }
        return;
    }

    if (m_waitingForCommitSnapshot) {
        m_waitingForCommitSnapshot = false;
        if (!lineageChanged && authoritativeId == m_selectedProfileId) {
            m_selectionDirty = false;
            setState(State::Ready);
        } else {
            m_selectionDirty = m_selectedProfileId != authoritativeId;
            setState(State::Conflict,
                     lineageChanged
                         ? QStringLiteral("Settings1 authority changed while applying")
                         : QStringLiteral("Profile selection changed after applying"));
        }
        Q_EMIT contentChanged();
        return;
    }

    if (m_selectionDirty || (m_editor && m_editor->dirty())) {
        m_selectionDirty = m_selectedProfileId != authoritativeId;
        setState(State::Conflict);
        Q_EMIT contentChanged();
        return;
    }
    m_selectionDirty = false;
    if (rebuild(*authoritative)) {
        setState(State::Ready);
    }
}

void CustomizeSettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (!saving()) {
        return;
    }
    if (outcome.status == SettingsWireStatus::Applied) {
        m_waitingForCommitSnapshot = true;
        return;
    }
    m_waitingForCommitSnapshot = false;
    setState(outcome.status == SettingsWireStatus::Conflict
                 ? State::Conflict
                 : State::Ready,
             outcome.message.isEmpty()
                 ? QStringLiteral("Settings1 rejected the profile selection")
                 : outcome.message);
}

void CustomizeSettingsModel::handleUncertain(const QString &message)
{
    if (!saving()) {
        return;
    }
    m_waitingForCommitSnapshot = false;
    setState(State::Unavailable,
             message.isEmpty()
                 ? QStringLiteral("The profile selection outcome is uncertain; refresh required")
                 : message);
    m_client.refresh();
}

void CustomizeSettingsModel::retry()
{
    m_client.refresh();
}

} // namespace QindaQt::Apps::SettingsCustomize
