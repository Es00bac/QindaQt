// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_appearance/appearance_preview.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"

#include "qindaqt/design_tokens/token_facade.h"

#include <QObject>
#include <QSet>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsAppearance {

// AGENT-CONTRACT: Projection of one SettingsClient scoped to
// AppearanceKeys::scopedKeys(). The rules follow the accepted Settings1
// controller contract (wiki/architecture/settings-service.md): last confirmed
// values are never claimed to be current authority during loss; owner loss
// forbids writes; an uncertain write is never replayed automatically; a
// conflict is private until a fresh baseline lands and then requires explicit
// user intent. QML must consume this model, never the raw settings client.
//
// Because the public client only commits one key per transaction, applyDraft()
// writes changed keys one at a time in AppearanceKeys::scopedKeys() order and
// reports truthful per-key outcomes; it never claims one atomic transaction.
class AppearanceSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
    Q_PROPERTY(bool conflict READ conflict NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable READ unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY stateChanged)
    Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY draftChanged)
    Q_PROPERTY(bool draftValid READ draftValid NOTIFY draftChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap fieldErrors READ fieldErrors NOTIFY draftChanged)
    Q_PROPERTY(QVariantList installedThemes READ installedThemes CONSTANT)
    Q_PROPERTY(QString resolvedThemeId READ resolvedThemeId NOTIFY previewChanged)
    Q_PROPERTY(bool configuredThemeInstalled READ configuredThemeInstalled
                   NOTIFY previewChanged)
    Q_PROPERTY(QString fallbackNotice READ fallbackNotice NOTIFY previewChanged)

public:
    // The facade pointer may be null in focused model tests; publication is a
    // preview affordance and never a precondition for truthful state.
    // AGENT-NOTE: The client type is spelled out fully because the unqualified
    //-looking `Services::SettingsClient` names the namespace, not the class.
    explicit AppearanceSettingsModel(
        QindaQt::Services::SettingsClient::SettingsClient &client,
        QVector<Themes::ThemeSpec> installedThemes,
        Qt::ColorScheme platformScheme,
        DesignTokens::TokenFacade *previewFacade = nullptr,
        QObject *parent = nullptr);

    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool saving() const noexcept;
    [[nodiscard]] bool conflict() const noexcept;
    [[nodiscard]] bool unavailable() const noexcept;
    [[nodiscard]] bool canEdit() const noexcept;
    [[nodiscard]] bool draftDirty() const noexcept;
    [[nodiscard]] bool draftValid() const noexcept;
    [[nodiscard]] bool applyAvailable() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QVariantMap draft() const;
    [[nodiscard]] QVariantMap fieldErrors() const;
    [[nodiscard]] QVariantList installedThemes() const;
    [[nodiscard]] QString resolvedThemeId() const;
    [[nodiscard]] bool configuredThemeInstalled() const;
    [[nodiscard]] QString fallbackNotice() const;

    // Coerces and stores one draft field. Returns false without changing the
    // draft when the key is unknown or the value does not fit the field type.
    Q_INVOKABLE bool setDraftValue(const QString &key, const QVariant &value);
    // Discards the whole draft and returns to last confirmed values. Rejected
    // while a commit sequence is in flight so cancel cannot fake success.
    Q_INVOKABLE bool cancelDraft();
    // Starts the per-key optimistic commit sequence for the dirty draft diff.
    Q_INVOKABLE bool applyDraft();
    // Safe transport/authority refresh; never resubmits a write.
    Q_INVOKABLE void retry();

Q_SIGNALS:
    void stateChanged();
    void draftChanged();
    void previewChanged();

private:
    enum class State { Loading, Ready, Saving, Conflict, Unavailable };

    struct CommitIntent final {
        QString key;
        QVariant value;
    };

    void handleClientState();
    void handleSnapshot();
    void handleCommit(
        const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
    void handleUncertain(const QString &message);

    void setState(State state, QString transientError = {});
    void setAuthorityReady(bool ready);
    void setConfirmed(AppearanceValues values);
    void refreshValidationAndPreview();
    void publishPreviewTokens();
    [[nodiscard]] bool startApplySequence();
    void writeNextQueuedKey();
    void abortSequence();
    [[nodiscard]] QSet<QString> installedThemeIds() const;

    QindaQt::Services::SettingsClient::SettingsClient &m_client;
    AppearancePreview m_preview;
    Qt::ColorScheme m_platformScheme;
    DesignTokens::TokenFacade *m_previewFacade = nullptr;

    State m_state = State::Loading;
    AppearanceValues m_confirmed;
    AppearanceValues m_draft;
    QString m_transientError;
    QString m_confirmedError;
    QString m_confirmedOwner;
    QString m_confirmedEpoch;
    AppearanceValidation m_validation;
    AppearanceResolution m_resolution;
    bool m_hasBaseline = false;
    bool m_authorityReady = false;
    bool m_sequenceActive = false;
    bool m_waitingFinalSnapshot = false;
    bool m_conflictIntent = false;
    QList<CommitIntent> m_queue;
};

} // namespace QindaQt::Apps::SettingsAppearance
