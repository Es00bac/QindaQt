// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_windows/windows_values.h>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsWindows {

// AGENT-CONTRACT: Same-thread projection of one borrowed public Settings1
// client scoped to WindowsKeys::scopedKeys(); the client must outlive the
// model. The rules follow the accepted Settings1 controller contract
// (wiki/architecture/settings-service.md), identical to the Accessibility
// route: last confirmed values are never claimed current during authority
// loss; owner loss forbids writes; an uncertain write is never replayed
// automatically; a conflict is private until a fresh baseline lands and then
// requires an explicit re-Apply.
//
// The public client commits one key per transaction, so applyDraft() writes
// the changed keys one at a time in scopedKeys() order from each fresh
// snapshot and never claims one atomic transaction. QML consumes this model
// only; it never sees the client, the transport, or the raw wire maps.
class WindowsSettingsModel final : public QObject {
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
    Q_PROPERTY(QString snapDistanceError READ snapDistanceError NOTIFY viewChanged)
    Q_PROPERTY(QString focusPolicy READ focusPolicy NOTIFY viewChanged)
    Q_PROPERTY(QString dockingModifier READ dockingModifier NOTIFY viewChanged)
    Q_PROPERTY(int snapDistance READ snapDistance NOTIFY viewChanged)
    Q_PROPERTY(QString closeContainerPolicy READ closeContainerPolicy NOTIFY viewChanged)
    Q_PROPERTY(QString draftFocusPolicy READ draftFocusPolicy NOTIFY viewChanged)
    Q_PROPERTY(QString draftDockingModifier READ draftDockingModifier NOTIFY viewChanged)
    Q_PROPERTY(int draftSnapDistance READ draftSnapDistance NOTIFY viewChanged)
    Q_PROPERTY(QString draftCloseContainerPolicy READ draftCloseContainerPolicy NOTIFY viewChanged)
    Q_PROPERTY(QVariantList focusPolicyChoices READ focusPolicyChoices CONSTANT)
    Q_PROPERTY(QVariantList dockingModifierChoices READ dockingModifierChoices CONSTANT)
    Q_PROPERTY(QVariantList closeContainerPolicyChoices READ closeContainerPolicyChoices CONSTANT)
    Q_PROPERTY(int minimumSnapDistance READ minimumSnapDistance CONSTANT)
    Q_PROPERTY(int maximumSnapDistance READ maximumSnapDistance CONSTANT)
    Q_PROPERTY(int defaultSnapDistance READ defaultSnapDistance CONSTANT)

public:
    enum class State { Loading, Ready, Saving, Conflict, Unavailable };

    explicit WindowsSettingsModel(QindaQt::Services::SettingsClient::SettingsClient &client,
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
    [[nodiscard]] QString snapDistanceError() const { return m_snapDistanceError; }
    [[nodiscard]] QString focusPolicy() const { return m_confirmed.focusPolicy; }
    [[nodiscard]] QString dockingModifier() const { return m_confirmed.dockingModifier; }
    [[nodiscard]] int snapDistance() const noexcept { return m_confirmed.snapDistance; }
    [[nodiscard]] QString closeContainerPolicy() const { return m_confirmed.closeContainerPolicy; }
    [[nodiscard]] QString draftFocusPolicy() const { return m_draft.focusPolicy; }
    [[nodiscard]] QString draftDockingModifier() const { return m_draft.dockingModifier; }
    [[nodiscard]] int draftSnapDistance() const noexcept { return m_draft.snapDistance; }
    [[nodiscard]] QString draftCloseContainerPolicy() const
    {
        return m_draft.closeContainerPolicy;
    }
    // Presentation lists of {token, label} in schema order; labels are the
    // page's only source so the tokens never leak into visible text.
    [[nodiscard]] static QVariantList focusPolicyChoices();
    [[nodiscard]] static QVariantList dockingModifierChoices();
    [[nodiscard]] static QVariantList closeContainerPolicyChoices();
    [[nodiscard]] static int minimumSnapDistance() noexcept
    {
        return WindowsValues::MinimumSnapDistance;
    }
    [[nodiscard]] static int maximumSnapDistance() noexcept
    {
        return WindowsValues::MaximumSnapDistance;
    }
    [[nodiscard]] static int defaultSnapDistance() noexcept
    {
        return WindowsValues::DefaultSnapDistance;
    }
    [[nodiscard]] const WindowsValues &confirmedValues() const noexcept { return m_confirmed; }
    [[nodiscard]] const WindowsValues &draftValues() const noexcept { return m_draft; }

    // Draft setters are refused (false) outside an editable state or for a
    // token the schema does not allow. The distance setter also refuses an
    // out-of-range value and publishes snapDistanceError.
    Q_INVOKABLE bool setDraftFocusPolicy(const QString &token);
    Q_INVOKABLE bool setDraftDockingModifier(const QString &token);
    Q_INVOKABLE bool setDraftSnapDistance(int distance);
    Q_INVOKABLE bool setDraftCloseContainerPolicy(const QString &token);
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
    void setConfirmed(const WindowsValues &values);
    void writeNextQueuedKey();
    void abortSequence();
    [[nodiscard]] bool setDraftToken(QString WindowsValues::*field, const QStringList &allowed,
                                     const QString &token);

    QindaQt::Services::SettingsClient::SettingsClient &m_client;
    State m_state = State::Loading;
    WindowsValues m_confirmed;
    WindowsValues m_draft;
    QString m_transientError;
    QString m_confirmedError;
    QString m_snapDistanceError;
    QString m_confirmedOwner;
    QString m_confirmedEpoch;
    QList<CommitIntent> m_queue;
    bool m_hasBaseline = false;
    bool m_authorityReady = false;
    bool m_sequenceActive = false;
    bool m_waitingFinalSnapshot = false;
    bool m_conflictIntent = false;
};

} // namespace QindaQt::Apps::SettingsWindows
