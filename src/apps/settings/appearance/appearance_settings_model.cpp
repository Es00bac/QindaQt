// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/themes/theme_spec.h"

#include <QSet>

#include <utility>

namespace QindaQt::Apps::SettingsAppearance {

using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsClient::SettingsClient;
using Services::SettingsProtocol::SettingsWireStatus;

namespace {
constexpr int MaximumDiagnosticLength = 512;

[[nodiscard]] QString statusFailureMessage(SettingsWireStatus status)
{
    return QStringLiteral("Settings save failed: %1")
        .arg(Services::SettingsProtocol::settingsWireStatusName(status));
}
} // namespace

AppearanceSettingsModel::AppearanceSettingsModel(
    SettingsClient &client, QVector<Themes::ThemeSpec> installedThemes,
    Qt::ColorScheme platformScheme, DesignTokens::TokenFacade *previewFacade,
    QObject *parent)
    : QObject(parent),
      m_client(client),
      m_preview(std::move(installedThemes)),
      m_platformScheme(platformScheme),
      m_previewFacade(previewFacade)
{
    connect(&m_client, &SettingsClient::stateChanged,
            this, &AppearanceSettingsModel::handleClientState);
    connect(&m_client, &SettingsClient::snapshotChanged,
            this, &AppearanceSettingsModel::handleSnapshot);
    connect(&m_client, &SettingsClient::commitFinished,
            this, &AppearanceSettingsModel::handleCommit);
    connect(&m_client, &SettingsClient::commitUncertain,
            this, &AppearanceSettingsModel::handleUncertain);

    m_validation = validateAppearanceDraft(m_draft, installedThemeIds());
    refreshValidationAndPreview();
}

bool AppearanceSettingsModel::loading() const noexcept
{
    return m_state == State::Loading;
}

bool AppearanceSettingsModel::ready() const noexcept
{
    return m_state == State::Ready;
}

bool AppearanceSettingsModel::saving() const noexcept
{
    return m_state == State::Saving;
}

bool AppearanceSettingsModel::conflict() const noexcept
{
    return m_state == State::Conflict;
}

bool AppearanceSettingsModel::unavailable() const noexcept
{
    return m_state == State::Unavailable;
}

bool AppearanceSettingsModel::canEdit() const noexcept
{
    return ready();
}

bool AppearanceSettingsModel::draftDirty() const noexcept
{
    return !(m_draft == m_confirmed);
}

bool AppearanceSettingsModel::draftValid() const noexcept
{
    return m_validation.valid;
}

bool AppearanceSettingsModel::applyAvailable() const
{
    return ready() && draftDirty() && draftValid();
}

QString AppearanceSettingsModel::statusText() const
{
    switch (m_state) {
    case State::Loading:
        return QStringLiteral("Loading appearance settings…");
    case State::Ready:
    case State::Saving:
        break;
    case State::Conflict:
        return QStringLiteral("Appearance changed elsewhere; current values reloaded");
    case State::Unavailable:
        return m_hasBaseline
            ? QStringLiteral("Last confirmed appearance settings retained; refresh to continue")
            : QStringLiteral("Appearance settings unavailable");
    }
    if (m_state == State::Saving) {
        return QStringLiteral("Saving appearance settings…");
    }
    return {};
}

QString AppearanceSettingsModel::errorText() const
{
    return m_transientError.isEmpty() ? m_confirmedError : m_transientError;
}

QVariantMap AppearanceSettingsModel::draft() const
{
    return m_draft.toVariantMap();
}

QVariantMap AppearanceSettingsModel::fieldErrors() const
{
    return m_validation.fieldErrors;
}

QVariantList AppearanceSettingsModel::installedThemes() const
{
    QVariantList entries;
    const auto &themes = m_preview.themes();
    const auto &maps = m_preview.previewMaps();
    entries.reserve(themes.size());
    for (int index = 0; index < themes.size(); ++index) {
        entries.append(QVariantMap{
            {QStringLiteral("id"), themes.at(index).id},
            {QStringLiteral("name"), themes.at(index).name},
            {QStringLiteral("variant"), themes.at(index).variant},
            {QStringLiteral("previewTokens"), maps.at(static_cast<size_t>(index))},
        });
    }
    return entries;
}

QString AppearanceSettingsModel::resolvedThemeId() const
{
    if (m_resolution.themeIndex < 0
        || m_resolution.themeIndex >= m_preview.themes().size()) {
        return {};
    }
    return m_preview.themes().at(m_resolution.themeIndex).id;
}

bool AppearanceSettingsModel::configuredThemeInstalled() const
{
    return m_resolution.configuredInstalled;
}

QString AppearanceSettingsModel::fallbackNotice() const
{
    if (m_resolution.configuredInstalled || m_resolution.themeIndex < 0) {
        return {};
    }
    return QStringLiteral(
               "Configured theme '%1' is not installed; previewing '%2'")
        .arg(m_draft.themeId, resolvedThemeId());
}

bool AppearanceSettingsModel::setDraftValue(const QString &key,
                                            const QVariant &value)
{
    if (!canEdit()) {
        return false;
    }

    AppearanceValues next = m_draft;
    // AGENT-GUARD: Field typing is strict — QVariant silently converts string
    // "12" to 12.0, and accepting that would let a page bug become a stored
    // wrong-typed value instead of a rejected draft write.
    const auto requireDouble = [&value](double *target) {
        switch (value.metaType().id()) {
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::LongLong:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::ULongLong:
            *target = value.toDouble();
            return true;
        default:
            return false;
        }
    };
    if (key == QLatin1String(AppearanceKeys::Theme)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        next.themeId = value.toString();
    } else if (key == QLatin1String(AppearanceKeys::ColorScheme)) {
        const auto scheme = colorSchemeFromToken(value.toString());
        if (!scheme.has_value()) return false;
        next.colorScheme = *scheme;
    } else if (key == QLatin1String(AppearanceKeys::FontFamily)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        next.fontFamily = value.toString();
    } else if (key == QLatin1String(AppearanceKeys::FontPointSize)) {
        if (!requireDouble(&next.fontPointSize)) return false;
    } else if (key == QLatin1String(AppearanceKeys::FontAntialiasing)) {
        if (value.metaType().id() != QMetaType::Bool) return false;
        next.fontAntialiasing = value.toBool();
    } else if (key == QLatin1String(AppearanceKeys::FontHinting)) {
        const auto hinting = fontHintingFromToken(value.toString());
        if (!hinting.has_value()) return false;
        next.fontHinting = *hinting;
    } else if (key == QLatin1String(AppearanceKeys::FontSubpixelOrder)) {
        const auto subpixel = subpixelOrderFromToken(value.toString());
        if (!subpixel.has_value()) return false;
        next.fontSubpixelOrder = *subpixel;
    } else if (key == QLatin1String(AppearanceKeys::Wallpaper)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        next.wallpaper = value.toString();
    } else if (key == QLatin1String(AppearanceKeys::WallpaperMode)) {
        const auto mode = wallpaperModeFromToken(value.toString());
        if (!mode.has_value()) return false;
        next.wallpaperMode = *mode;
    } else if (key == QLatin1String(AppearanceKeys::UiScale)) {
        if (!requireDouble(&next.uiScale)) return false;
    } else {
        return false;
    }

    if (next == m_draft) {
        return true;
    }
    m_draft = next;
    refreshValidationAndPreview();
    return true;
}

bool AppearanceSettingsModel::cancelDraft()
{
    if (!ready() || !draftDirty()) {
        return false;
    }
    m_draft = m_confirmed;
    refreshValidationAndPreview();
    return true;
}

bool AppearanceSettingsModel::applyDraft()
{
    if (!applyAvailable()) {
        return false;
    }
    return startApplySequence();
}

void AppearanceSettingsModel::retry()
{
    // Do not claim progress before the client actually retries; a repeated
    // synchronous start failure must keep the Unavailable truth visible.
    m_client.refresh();
}

QSet<QString> AppearanceSettingsModel::installedThemeIds() const
{
    QSet<QString> ids;
    const auto &themes = m_preview.themes();
    ids.reserve(themes.size());
    for (const auto &theme : themes) {
        ids.insert(theme.id);
    }
    return ids;
}

void AppearanceSettingsModel::handleClientState()
{
    switch (m_client.state()) {
    case ClientState::Ready:
        // Snapshot handling promotes to Ready once fresh authority lands.
        // Keep Saving/Conflict intent private until that resolves them.
        break;
    case ClientState::Authenticating:
        // A retained baseline is not authority. Any in-flight sequence dies
        // here without replay; the draft (user intent) is kept for review.
        abortSequence();
        setState(m_hasBaseline ? State::Unavailable : State::Loading,
                 m_client.lastError());
        break;
    case ClientState::Unavailable:
    case ClientState::Degraded:
        abortSequence();
        setState(State::Unavailable, m_client.lastError());
        break;
    }
}

void AppearanceSettingsModel::handleSnapshot()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot) {
        return;
    }
    QString error;
    const auto decoded =
        AppearanceValues::fromVariantMap(snapshot->values, &error);
    if (!decoded.has_value()) {
        abortSequence();
        setState(State::Unavailable,
                 error.isEmpty()
                     ? QStringLiteral("Appearance settings have an invalid value")
                     : error);
        return;
    }
    setConfirmed(*decoded);

    if (m_waitingFinalSnapshot) {
        m_waitingFinalSnapshot = false;
        if (*decoded == m_draft) {
            m_conflictIntent = false;
            setState(State::Ready);
        } else {
            // Another writer changed an appearance key after our commit.
            m_conflictIntent = true;
            setState(State::Conflict);
        }
        return;
    }
    if (m_sequenceActive) {
        // AGENT-GUARD: The client refreshes authority after every commit and
        // refuses writes until it is Ready again. Issue the next queued key
        // only here, from the fresh baseline, or every second write would
        // carry a stale base revision and fail as Conflict.
        if (m_client.state() != ClientState::Ready) {
            return;
        }
        writeNextQueuedKey();
        return;
    }
    if (m_conflictIntent) {
        if (*decoded == m_draft) {
            m_conflictIntent = false;
            setState(State::Ready);
        } else if (m_state != State::Conflict) {
            // Fresh authority confirms the unresolved conflict after a loss.
            setState(State::Conflict);
        }
        return;
    }
    setState(State::Ready);
}

void AppearanceSettingsModel::handleCommit(const CommitOutcome &outcome)
{
    if (!m_sequenceActive || m_queue.isEmpty()) {
        return;
    }

    const CommitIntent intended = m_queue.first();
    if (outcome.status == SettingsWireStatus::Applied) {
        m_queue.removeFirst();
        // AGENT-GUARD: Never write the next key from the commit reply. The
        // client still holds the pre-commit base revision until its automatic
        // refresh lands, and the service would reject the write as Conflict.
        // The fresh snapshot handler continues the sequence.
        if (m_queue.isEmpty()) {
            m_sequenceActive = false;
            m_waitingFinalSnapshot = true;
        }
        return;
    }

    if (outcome.status == SettingsWireStatus::Conflict) {
        const QVariant current = outcome.currentValues.value(intended.key);
        if (current == intended.value) {
            // The queued value already is authority; treat the key as done
            // once the matching fresh snapshot confirms it.
            m_queue.removeFirst();
            if (m_queue.isEmpty()) {
                m_sequenceActive = false;
                m_waitingFinalSnapshot = true;
            }
            return;
        }
        abortSequence();
        m_conflictIntent = true;
        setState(State::Conflict);
        return;
    }

    // Validation/read-only/persistence/exhaustion/unknown-key failures are
    // confirmed rejections: keep the diagnostic visible across rebaseline and
    // never resubmit. A new explicit Apply after fresh authority dismisses it.
    abortSequence();
    m_confirmedError =
        outcome.message.isEmpty() ? statusFailureMessage(outcome.status)
                                  : outcome.message.left(MaximumDiagnosticLength);
    setState(State::Ready);
}

void AppearanceSettingsModel::handleUncertain(const QString &message)
{
    // Timeout, owner replacement, or transport loss during a write is never
    // retried automatically; the user must refresh and re-apply explicitly.
    abortSequence();
    setState(State::Unavailable,
             message.isEmpty()
                 ? QStringLiteral("Appearance commit outcome is uncertain")
                 : message.left(MaximumDiagnosticLength));
}

void AppearanceSettingsModel::setState(State state, QString transientError)
{
    // AGENT-GUARD: Transient errors are replaced on every state change, while
    // m_confirmedError is only cleared by an explicit new apply sequence.
    // Clearing it on automatic authority refresh would hide a confirmed
    // rejection the user has not yet been able to answer.
    const QString nextError = std::move(transientError);
    if (m_state == state && m_transientError == nextError) {
        return;
    }
    m_state = state;
    m_transientError = nextError;
    Q_EMIT stateChanged();
}

void AppearanceSettingsModel::setConfirmed(AppearanceValues values)
{
    const bool baselineBefore = m_hasBaseline;
    const bool changed = !(m_confirmed == values);
    m_confirmed = values;
    if (!baselineBefore) {
        m_hasBaseline = true;
    }
    if (changed || !baselineBefore) {
        refreshValidationAndPreview();
    }
}

void AppearanceSettingsModel::refreshValidationAndPreview()
{
    m_validation = validateAppearanceDraft(m_draft, installedThemeIds());
    m_resolution = m_preview.resolve(m_draft, m_platformScheme);
    publishPreviewTokens();
    // AGENT-NOTE: applyAvailable is a composite of state, draft dirt, and
    // draft validity but a Q_PROPERTY allows one NOTIFY signal; stateChanged
    // doubles as its change notification here.
    Q_EMIT stateChanged();
    Q_EMIT draftChanged();
    Q_EMIT previewChanged();
}

void AppearanceSettingsModel::publishPreviewTokens()
{
    if (m_previewFacade == nullptr || m_resolution.themeIndex < 0) {
        return;
    }
    const auto &theme = m_preview.themes().at(m_resolution.themeIndex);
    // AGENT-GUARD: Publication must always carry one complete immutable
    // generation derived from the draft; publishing derived roles piecemeal
    // would let controls render a hybrid of two themes.
    const auto derived = DesignTokens::DesignTokenDeriver::derive(
        theme, m_preview.accessibilityInputs(m_draft, theme));
    if (derived.ok()) {
        QString error;
        m_previewFacade->publish(derived.tokens, &error);
    }
}

bool AppearanceSettingsModel::startApplySequence()
{
    const QVariantMap draftMap = m_draft.toVariantMap();
    const QVariantMap confirmedMap = m_confirmed.toVariantMap();
    for (const QString &key : AppearanceKeys::scopedKeys()) {
        const QVariant next = draftMap.value(key);
        if (next != confirmedMap.value(key)) {
            m_queue.append(CommitIntent{key, next});
        }
    }
    if (m_queue.isEmpty()) {
        return false;
    }
    m_sequenceActive = true;
    m_conflictIntent = false;
    m_confirmedError.clear();
    setState(State::Saving);
    writeNextQueuedKey();
    // A synchronous write refusal either aborts the sequence with an honest
    // state change (queued keys cleared) or waits for fresh authority; both
    // keep Saving honest through the state signal, so report what happened.
    return m_sequenceActive;
}

void AppearanceSettingsModel::writeNextQueuedKey()
{
    if (m_queue.isEmpty()) {
        return;
    }
    QString error;
    const CommitIntent intent = m_queue.first();
    if (m_client.setUserValue(intent.key, intent.value, &error)) {
        return;
    }
    if (m_client.state() == ClientState::Ready) {
        // Ready but refusing this key is a caller error; publish it as truth
        // instead of spinning or silently dropping the user's Apply intent.
        abortSequence();
        setState(State::Unavailable, error.left(MaximumDiagnosticLength));
        return;
    }
    // Authority is refreshing (Authenticating/Degraded); the matching fresh
    // snapshot or the state-change path continues or aborts the sequence.
}

void AppearanceSettingsModel::abortSequence()
{
    m_queue.clear();
    m_sequenceActive = false;
    m_waitingFinalSnapshot = false;
}

} // namespace QindaQt::Apps::SettingsAppearance
