// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_customize/customize_editor_host.h"

#include <QObject>
#include <QVariantList>

#include <functional>
#include <memory>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
}

namespace QindaQt::Apps::SettingsCustomize {

inline constexpr QLatin1StringView LayoutProfileSettingsKey("panels.layoutProfile");

using EditorHostFactory = std::function<std::unique_ptr<CustomizeEditorHost>(
    const Profiles::LayoutProfile &profile)>;

// QObject projection for the compiled Customize page. The model owns route
// state but receives Settings1 and editor-host dependencies explicitly; QML
// never sees a transport, repository, or editing command.
class CustomizeSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
    Q_PROPERTY(bool conflict READ conflict NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable READ unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY contentChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY contentChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY contentChanged)
    Q_PROPERTY(bool visualDragActive READ visualDragActive NOTIFY contentChanged)
    Q_PROPERTY(bool dropAccepted READ dropAccepted NOTIFY contentChanged)
    Q_PROPERTY(QString dropReason READ dropReason NOTIFY contentChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString announcement READ announcement NOTIFY announcementChanged)
    Q_PROPERTY(QString selectedProfileId READ selectedProfileId NOTIFY contentChanged)
    Q_PROPERTY(QVariantList profiles READ profiles CONSTANT)
    Q_PROPERTY(QVariantList panels READ panels NOTIFY contentChanged)
    Q_PROPERTY(QVariantList palette READ palette CONSTANT)
    Q_PROPERTY(QString selectedKind READ selectedKind NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPanelId READ selectedPanelId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAppletId READ selectedAppletId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedProperties READ selectedProperties NOTIFY selectionChanged)

public:
    enum class State { Loading, Ready, Saving, Conflict, Unavailable };
    Q_ENUM(State)

    // AGENT-CONTRACT: client must outlive this GUI-thread model. The factory
    // returns a fresh editor host for profile selection and discard rebuilds;
    // returning null fails closed and preserves the last confirmed selection.
    CustomizeSettingsModel(
        Services::SettingsClient::SettingsClient &client,
        QVector<Profiles::LayoutProfile> availableProfiles,
        QVector<Applets::AppletManifest> manifests,
        EditorHostFactory hostFactory,
        QString startupError = {},
        QObject *parent = nullptr);

    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool saving() const noexcept;
    [[nodiscard]] bool conflict() const noexcept;
    [[nodiscard]] bool unavailable() const noexcept;
    [[nodiscard]] bool canEdit() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] bool applyAvailable() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool visualDragActive() const noexcept;
    [[nodiscard]] bool dropAccepted() const noexcept;
    [[nodiscard]] QString dropReason() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString announcement() const { return m_announcement; }
    [[nodiscard]] QString selectedProfileId() const { return m_selectedProfileId; }
    [[nodiscard]] QVariantList profiles() const;
    [[nodiscard]] QVariantList panels() const;
    [[nodiscard]] QVariantList palette() const;
    [[nodiscard]] QString selectedKind() const { return m_selectedKind; }
    [[nodiscard]] QString selectedPanelId() const { return m_selectedPanelId; }
    [[nodiscard]] QString selectedAppletId() const { return m_selectedAppletId; }
    [[nodiscard]] QVariantMap selectedProperties() const;

    Q_INVOKABLE bool selectProfile(const QString &profileId);
    Q_INVOKABLE void selectPanel(const QString &panelId);
    Q_INVOKABLE void selectApplet(const QString &panelId, const QString &appletId);
    Q_INVOKABLE bool startPaletteDrag(const QString &pluginId);
    Q_INVOKABLE bool startAppletDrag(const QString &panelId, const QString &appletId);
    Q_INVOKABLE bool hoverDropTarget(const QString &panelId, const QString &zone,
                                     const QString &beforeAppletId = {});
    Q_INVOKABLE bool commitDrag();
    Q_INVOKABLE bool cancelDrag();
    Q_INVOKABLE bool keyboardInsert(const QString &pluginId,
                                    const QString &panelId,
                                    const QString &zone,
                                    const QString &beforeAppletId = {});
    Q_INVOKABLE bool keyboardMoveMode();
    Q_INVOKABLE bool keyboardStep(const QString &direction);
    Q_INVOKABLE bool removeSelected();
    Q_INVOKABLE bool duplicateSelected();
    Q_INVOKABLE bool configureSelectedPanel(const QString &field,
                                            const QVariant &value);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE bool apply();
    Q_INVOKABLE bool discard();
    Q_INVOKABLE void retry();

Q_SIGNALS:
    void stateChanged();
    void contentChanged();
    void selectionChanged();
    void announcementChanged();

private:
    void handleClientState();
    void handleSnapshot();
    void handleCommit(const Services::SettingsClient::CommitOutcome &outcome);
    void handleUncertain(const QString &message);
    [[nodiscard]] const Profiles::LayoutProfile *findProfile(const QString &id) const;
    [[nodiscard]] const Applets::AppletManifest *findManifest(const QString &id) const;
    [[nodiscard]] bool rebuild(const Profiles::LayoutProfile &profile);
    [[nodiscard]] bool settleEditorOutcome(
        const ShellCustomizationEditor::EditorOutcome &outcome,
        const QString &successAnnouncement = {});
    void refreshProjection();
    void setState(State state, QString error = {});
    void clearSelectionIfMissing();
    [[nodiscard]] ShellCustomizationEditor::DropTarget targetFromStrings(
        const QString &panelId, const QString &zone,
        const QString &beforeAppletId) const;
    [[nodiscard]] QString nextDuplicateId(const QString &base) const;

    Services::SettingsClient::SettingsClient &m_client;
    QVector<Profiles::LayoutProfile> m_profiles;
    QVector<Applets::AppletManifest> m_manifests;
    EditorHostFactory m_hostFactory;
    std::unique_ptr<CustomizeEditorHost> m_editor;
    State m_state = State::Loading;
    QString m_startupError;
    QString m_error;
    QString m_confirmedProfileId;
    QString m_selectedProfileId;
    QString m_selectedKind;
    QString m_selectedPanelId;
    QString m_selectedAppletId;
    QString m_announcement;
    QString m_draggedName;
    QString m_lastDropReason;
    QString m_confirmedOwner;
    QString m_confirmedEpoch;
    ShellCustomizationEditor::DropTarget m_keyboardTarget;
    bool m_hasBaseline = false;
    bool m_selectionDirty = false;
    bool m_waitingForCommitSnapshot = false;
    bool m_keyboardMoving = false;
    bool m_lastDropAccepted = false;
    bool m_editorUnavailable = false;
};

} // namespace QindaQt::Apps::SettingsCustomize
