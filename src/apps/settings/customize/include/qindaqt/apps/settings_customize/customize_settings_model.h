// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_customize/customize_preset_catalog.h"
#include "qindaqt/profiles/user_profile_store.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <optional>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsCustomize {

inline constexpr QLatin1StringView LayoutProfileSettingsKey("panels.layoutProfile");
inline constexpr QLatin1StringView PanelHideDelaySettingsKey("panels.autoHideDelayMs");
// AGENT-CONTRACT: the layout a new user gets (ADR-0263). It must equal the
// shell's DefaultLayoutProfileId and the panels.layoutProfile default in
// data/settings/schema-v2.json; tst_customize_presets checks the schema.
// Deleting the active preset switches here first (ADR-0267).
inline constexpr QLatin1StringView DefaultLayoutPresetId("macos-inspired");
inline constexpr int MaximumPresetNameLength = 64;
inline constexpr int MaximumUserPresets = 50;

// Where presets come from: the installed profile directories (low-to-high
// precedence, never the user store) and the writable user store, which the
// shell also reads last and which panel edits write (ADR-0213, ADR-0266).
struct PresetLocations final {
    QStringList stockDirectories;
    QString userDirectory;
};

// QObject projection for the Settings Customize page: a gallery of layout
// presets (ADR-0267). Clicking a preset commits the Settings1
// panels.layoutProfile selection and the running shell adopts it live
// (ADR-0122); the user's own presets are copies in the user profile store.
// Layout editing itself happens on the panels, never here.
//
// AGENT-CONTRACT: GUI thread only; `client` is borrowed and must outlive the
// model. Every action returns true only once its authority accepted it: a
// store write or removal succeeded, or Settings1 accepted the selection
// commit (the switch itself is reported only after readback confirms it).
// QML never sees a transport, a file path, or a profile document.
class CustomizeSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable READ unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    // AGENT-CONTRACT: the Settings Center's Customize departure fence
    // (SettingsRouteHost, Main.qml) reads `dirty` from this model. Presets
    // have no draft (every action goes straight to its authority), so it is
    // always false and leaving or closing never prompts.
    Q_PROPERTY(bool dirty READ dirty CONSTANT)
    Q_PROPERTY(bool canSwitch READ canSwitch NOTIFY stateChanged)
    Q_PROPERTY(bool canManage READ canManage NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY stateChanged)
    Q_PROPERTY(QString activePresetId READ activePresetId NOTIFY presetsChanged)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(QString defaultPresetId READ defaultPresetId CONSTANT)
    Q_PROPERTY(int maximumNameLength READ maximumNameLength CONSTANT)
    Q_PROPERTY(bool panelHideDelayAvailable READ panelHideDelayAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool panelHideDelayEditable READ panelHideDelayEditable NOTIFY stateChanged)
    Q_PROPERTY(bool panelHideDelayPending READ panelHideDelayPending NOTIFY stateChanged)
    Q_PROPERTY(int panelHideDelayMs READ panelHideDelayMs NOTIFY stateChanged)
    Q_PROPERTY(QString panelHideDelayStatus READ panelHideDelayStatus NOTIFY stateChanged)

public:
    enum class State { Loading, Ready, Unavailable };
    Q_ENUM(State)

    CustomizeSettingsModel(Services::SettingsClient::SettingsClient &client,
                           PresetLocations locations,
                           QObject *parent = nullptr);

    [[nodiscard]] bool loading() const noexcept { return m_state == State::Loading; }
    [[nodiscard]] bool ready() const noexcept { return m_state == State::Ready; }
    [[nodiscard]] bool unavailable() const noexcept { return m_state == State::Unavailable; }
    [[nodiscard]] bool busy() const noexcept { return m_pendingSelection.has_value(); }
    [[nodiscard]] bool dirty() const noexcept { return false; }
    [[nodiscard]] bool canSwitch() const noexcept;
    [[nodiscard]] bool canManage() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const { return m_error; }
    [[nodiscard]] QString noticeText() const { return m_notice; }
    [[nodiscard]] QString activePresetId() const { return m_activeId; }
    [[nodiscard]] QVariantList presets() const;
    [[nodiscard]] QString defaultPresetId() const { return QString(DefaultLayoutPresetId); }
    [[nodiscard]] int maximumNameLength() const noexcept { return MaximumPresetNameLength; }
    [[nodiscard]] bool panelHideDelayAvailable() const noexcept;
    [[nodiscard]] bool panelHideDelayEditable() const noexcept;
    [[nodiscard]] bool panelHideDelayPending() const noexcept;
    [[nodiscard]] int panelHideDelayMs() const noexcept;
    [[nodiscard]] QString panelHideDelayStatus() const;

    // Commits panels.layoutProfile. False when nothing was submitted.
    Q_INVOKABLE bool activatePreset(const QString &presetId);
    // Empty when `name` may name a new or renamed preset; otherwise the
    // reason, for the name dialog. `renamingId` is exempt from uniqueness.
    Q_INVOKABLE QString presetNameError(const QString &name,
                                        const QString &renamingId = QString()) const;
    // Copies a preset's current content (for the active one: the applied
    // layout with every edit made on the panels) into a new own preset.
    Q_INVOKABLE bool savePresetAs(const QString &sourceId, const QString &name);
    Q_INVOKABLE bool duplicatePreset(const QString &presetId);
    Q_INVOKABLE bool renamePreset(const QString &presetId, const QString &name);
    // Own presets only. Deleting the active one first switches to the
    // default preset and removes the file only after that switch is
    // confirmed, so neither Settings1 nor the shell ever names a missing
    // layout.
    Q_INVOKABLE bool deletePreset(const QString &presetId);
    // Modified built-ins only: removes the user copy so the installed layout
    // shows through again (the shell adopts it through its store watcher).
    Q_INVOKABLE bool restorePreset(const QString &presetId);
    // AGENT-CONTRACT: This writes only the existing global Settings1 delay.
    // False means no write was admitted; success still requires same-owner,
    // same-epoch readback at the commit's observed revision.
    Q_INVOKABLE bool setPanelHideDelayMs(int milliseconds);
    Q_INVOKABLE void retry();
    // Re-reads the installed catalog and the user store. The store watcher
    // calls it after panel edits land; copies, renames and restores call it
    // first so they work from the newest direct edits.
    void reloadPresets();

Q_SIGNALS:
    void stateChanged();
    void presetsChanged();

private:
    struct PendingSelection final {
        QString requestedId;
        QString deleteAfter;  // own preset removed once the switch is confirmed
        QString owner;
        QString epoch;
        quint64 readbackFloor = 0;
        bool awaitingReadback = false;
    };

    void handleClientState();
    void handleSnapshot();
    void handleCommit(const Services::SettingsClient::CommitOutcome &outcome);
    void handleUncertain(const QString &message);
    [[nodiscard]] bool beginSelection(const QString &presetId, const QString &deleteAfter);
    void settleSelection(bool confirmed, const QString &message);
    void handleDelaySnapshot();
    void handleDelayClientState();
    void handleDelayCommit(const Services::SettingsClient::CommitOutcome &outcome);
    void handleDelayUncertain(const QString &message);
    void finishDelayUncertain(const QString &message);
    void clearDelayPending();
    void refreshStoreWatch();
    void updateState();
    void report(QString notice, QString error = {});
    [[nodiscard]] const LayoutPreset *findPreset(const QString &presetId) const;
    [[nodiscard]] QString presetName(const QString &presetId) const;
    [[nodiscard]] bool copyPreset(const QString &sourceId, const QString &name);
    [[nodiscard]] bool removeUserCopy(const QString &presetId, const QString &notice);
    [[nodiscard]] QString nextUserPresetId(const QString &name) const;
    [[nodiscard]] QString copyName(const QString &name) const;
    [[nodiscard]] QString fallbackPresetId(const QString &excludedId) const;
    [[nodiscard]] int ownPresetCount() const;

    Services::SettingsClient::SettingsClient &m_client;
    PresetLocations m_locations;
    Profiles::UserProfileStore m_store;
    QVector<LayoutPreset> m_presets;
    QString m_catalogError;
    QFileSystemWatcher m_storeWatch;
    QTimer m_reloadDebounce;
    State m_state = State::Loading;
    QString m_stateReason;
    QString m_selectionError;
    QString m_error;
    QString m_notice;
    QString m_activeId;
    bool m_hasSelection = false;
    std::optional<PendingSelection> m_pendingSelection;
    QTimer m_selectionReadbackDeadline;
    struct PendingDelay final {
        int requestedMs = 0;
        QString owner;
        QString epoch;
        quint64 initiatingRevision = 0;
        quint64 readbackFloor = 0;
        bool awaitingReadback = false;
    };
    std::optional<PendingDelay> m_pendingDelay;
    QTimer m_delayReadbackRetry;
    QTimer m_delayReadbackDeadline;
    int m_confirmedDelayMs = 250;
    bool m_delayHasBaseline = false;
    QString m_delayOwner;
    QString m_delayEpoch;
    QString m_delayError;
};

} // namespace QindaQt::Apps::SettingsCustomize
