// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_accessibility/accessibility_values.h>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsAccessibility {

// AGENT-CONTRACT: Same-thread projection of one borrowed public Settings1
// client scoped to AccessibilityKeys::scopedKeys(); the client must outlive
// the model. The rules follow the accepted Settings1 controller contract
// (wiki/architecture/settings-service.md): last confirmed values are never
// claimed current during authority loss; owner loss forbids writes; an
// uncertain write is never replayed automatically; a conflict is private
// until a fresh baseline lands and then requires an explicit re-Apply.
//
// The public client commits one key per transaction, so applyDraft() writes
// the changed keys one at a time in scopedKeys() order from each fresh
// snapshot and never claims one atomic transaction. QML consumes this model
// only; it never sees the client, the transport, or the raw wire maps.
class AccessibilitySettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY viewChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY viewChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY viewChanged)
    Q_PROPERTY(bool conflict READ conflict NOTIFY viewChanged)
    Q_PROPERTY(bool unavailable READ unavailable NOTIFY viewChanged)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY viewChanged)
    Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY viewChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY viewChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
    Q_PROPERTY(QString textScaleError READ textScaleError NOTIFY viewChanged)
    Q_PROPERTY(bool highContrast READ highContrast NOTIFY viewChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY viewChanged)
    Q_PROPERTY(bool reducedTransparency READ reducedTransparency NOTIFY viewChanged)
    Q_PROPERTY(double textScale READ textScale NOTIFY viewChanged)
    Q_PROPERTY(bool draftHighContrast READ draftHighContrast NOTIFY viewChanged)
    Q_PROPERTY(bool draftReducedMotion READ draftReducedMotion NOTIFY viewChanged)
    Q_PROPERTY(bool draftReducedTransparency READ draftReducedTransparency NOTIFY viewChanged)
    Q_PROPERTY(double draftTextScale READ draftTextScale NOTIFY viewChanged)
    Q_PROPERTY(double minimumTextScale READ minimumTextScale CONSTANT)
    Q_PROPERTY(double maximumTextScale READ maximumTextScale CONSTANT)
    Q_PROPERTY(double defaultTextScale READ defaultTextScale CONSTANT)

public:
    enum class State { Loading, Ready, Saving, Conflict, Unavailable };

    explicit AccessibilitySettingsModel(
        QindaQt::Services::SettingsClient::SettingsClient &client,
        QObject *parent = nullptr);

    [[nodiscard]] bool loading() const noexcept { return m_state == State::Loading; }
    [[nodiscard]] bool ready() const noexcept { return m_state == State::Ready; }
    [[nodiscard]] bool saving() const noexcept { return m_state == State::Saving; }
    [[nodiscard]] bool conflict() const noexcept { return m_state == State::Conflict; }
    [[nodiscard]] bool unavailable() const noexcept { return m_state == State::Unavailable; }
    [[nodiscard]] bool canEdit() const noexcept;
    [[nodiscard]] bool draftDirty() const noexcept;
    [[nodiscard]] bool applyAvailable() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString textScaleError() const { return m_textScaleError; }
    [[nodiscard]] bool highContrast() const noexcept { return m_confirmed.highContrast; }
    [[nodiscard]] bool reducedMotion() const noexcept { return m_confirmed.reducedMotion; }
    [[nodiscard]] bool reducedTransparency() const noexcept
    {
        return m_confirmed.reducedTransparency;
    }
    [[nodiscard]] double textScale() const noexcept { return m_confirmed.textScale; }
    [[nodiscard]] bool draftHighContrast() const noexcept { return m_draft.highContrast; }
    [[nodiscard]] bool draftReducedMotion() const noexcept { return m_draft.reducedMotion; }
    [[nodiscard]] bool draftReducedTransparency() const noexcept
    {
        return m_draft.reducedTransparency;
    }
    [[nodiscard]] double draftTextScale() const noexcept { return m_draft.textScale; }
    [[nodiscard]] static double minimumTextScale() noexcept
    {
        return AccessibilityValues::MinimumTextScale;
    }
    [[nodiscard]] static double maximumTextScale() noexcept
    {
        return AccessibilityValues::MaximumTextScale;
    }
    [[nodiscard]] static double defaultTextScale() noexcept
    {
        return AccessibilityValues::DefaultTextScale;
    }
    [[nodiscard]] const AccessibilityValues &confirmedValues() const noexcept
    {
        return m_confirmed;
    }
    [[nodiscard]] const AccessibilityValues &draftValues() const noexcept
    {
        return m_draft;
    }

    // Draft setters are refused (false) outside an editable state. The scale
    // setter also refuses an out-of-range value and publishes textScaleError.
    Q_INVOKABLE bool setDraftHighContrast(bool enabled);
    Q_INVOKABLE bool setDraftReducedMotion(bool enabled);
    Q_INVOKABLE bool setDraftReducedTransparency(bool enabled);
    Q_INVOKABLE bool setDraftTextScale(double scale);
    // Starts the per-key commit sequence for the changed keys. In Conflict
    // this is the explicit "apply my choice" the Settings1 contract requires.
    Q_INVOKABLE bool applyDraft();
    // Returns the draft to the last confirmed values; refused while Saving.
    Q_INVOKABLE bool revertDraft();
    // Safe authority refresh; never resubmits a write.
    Q_INVOKABLE void retry();

Q_SIGNALS:
    void viewChanged();

private:
    struct CommitIntent final {
        QString key;
        QVariant value;
    };

    void handleClientState();
    void handleSnapshot();
    void handleCommit(const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
    void handleUncertain(const QString &message);
    void setState(State state, QString transientError = {});
    void setAuthorityReady(bool ready);
    void setConfirmed(const AccessibilityValues &values);
    void writeNextQueuedKey();
    void abortSequence();
    [[nodiscard]] bool setDraftFlag(bool AccessibilityValues::*field, bool enabled);

    QindaQt::Services::SettingsClient::SettingsClient &m_client;
    State m_state = State::Loading;
    AccessibilityValues m_confirmed;
    AccessibilityValues m_draft;
    QString m_transientError;
    QString m_confirmedError;
    QString m_textScaleError;
    QString m_confirmedOwner;
    QString m_confirmedEpoch;
    QList<CommitIntent> m_queue;
    bool m_hasBaseline = false;
    bool m_authorityReady = false;
    bool m_sequenceActive = false;
    bool m_waitingFinalSnapshot = false;
    bool m_conflictIntent = false;
};

} // namespace QindaQt::Apps::SettingsAccessibility
