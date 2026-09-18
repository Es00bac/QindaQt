// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <memory>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}
namespace QindaQt::ShellCustomizationEditor {
class LiveEditorHost;
struct EditorOutcome;
struct DropTarget;
}

namespace QindaQt::Shell {

class SettingsRouteLauncher;

// The shell side of in-place customization (ADR "host the customization
// editor live in the shell"). It owns one LiveEditorHost built from the live
// layout profile, the shell's output inventory, the manifest catalog and the
// user profile store, and maps every Meta+right-click menu entry and every
// edit-mode gesture onto one editor gesture followed by one Apply. The shell
// adopts the written profile through its existing store watcher, so this
// class never rebuilds panels itself.
//
// AGENT-CONTRACT: GUI thread only. Every Q_INVOKABLE returns true only when
// the engine accepted the intent AND the profile was written; statusText
// carries the typed reason otherwise. The chord is one Settings1 key
// (shell.customization.chord) read through a purpose-scoped client so an
// absent key can never poison another preference scope.
class LiveCustomizationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool editMode READ editMode NOTIFY editModeChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool dragActive READ dragActive NOTIFY changed)
    Q_PROPERTY(bool dropAccepted READ dropAccepted NOTIFY changed)
    Q_PROPERTY(QString dropReason READ dropReason NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString chord READ chord NOTIFY chordChanged)
    Q_PROPERTY(int chordModifiers READ chordModifiers NOTIFY chordChanged)
    Q_PROPERTY(bool customizeRouteAvailable READ customizeRouteAvailable CONSTANT)

public:
    using OutputSource = std::function<QVector<ShellLayout::LogicalOutput>()>;

    [[nodiscard]] static QString chordSettingsKey();
    [[nodiscard]] static QString defaultChord();
    [[nodiscard]] static int modifiersForChord(const QString &chord);

    // `settings` may be null (no Settings1 scope: the default chord holds);
    // `routes` may be null (Open Customize and Change wallpaper report
    // failure). Both are borrowed and must outlive this controller.
    LiveCustomizationController(const QVector<Applets::AppletManifest> &manifests,
                                QString userProfileDirectory,
                                OutputSource outputs,
                                Services::SettingsClient::SettingsClient *settings,
                                SettingsRouteLauncher *routes,
                                QObject *parent = nullptr);
    ~LiveCustomizationController() override;

    // Follows the shell's layout adoption. A profile equal to the session's
    // committed profile keeps the session (and its undo history); anything
    // else rebuilds the host from the adopted profile.
    void adoptProfile(const Profiles::LayoutProfile &profile);
    // Output hotplug stales the session; the next action rebuilds it.
    void outputGenerationChanged();

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool editMode() const noexcept { return m_editMode; }
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool dragActive() const;
    [[nodiscard]] bool dropAccepted() const noexcept { return m_dropAccepted; }
    [[nodiscard]] QString dropReason() const { return m_dropReason; }
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] QString chord() const { return m_chord; }
    [[nodiscard]] int chordModifiers() const { return modifiersForChord(m_chord); }
    [[nodiscard]] bool customizeRouteAvailable() const noexcept { return m_routes != nullptr; }
    [[nodiscard]] const ShellCustomizationEditor::LiveEditorHost *host() const noexcept
    {
        return m_host.get();
    }

    // -- applet menu --
    Q_INVOKABLE QString appletDisplayName(const QString &panelId, const QString &appletId) const;
    Q_INVOKABLE bool moveAppletToZone(const QString &panelId, const QString &appletId,
                                      const QString &zone);
    Q_INVOKABLE bool moveAppletStep(const QString &panelId, const QString &appletId, int delta);
    Q_INVOKABLE bool moveAppletToPanel(const QString &panelId, const QString &appletId,
                                       const QString &targetPanelId);
    Q_INVOKABLE bool removeApplet(const QString &panelId, const QString &appletId);
    Q_INVOKABLE QVariantList appletSettingRows(const QString &panelId,
                                               const QString &appletId) const;
    Q_INVOKABLE bool setAppletSetting(const QString &panelId, const QString &appletId,
                                      const QString &key, const QVariant &value);
    // -- panel menu --
    Q_INVOKABLE QVariantList palette(const QString &panelId) const;
    Q_INVOKABLE bool addApplet(const QString &panelId, const QString &zone,
                               const QString &pluginId);
    Q_INVOKABLE QVariantMap panelOptions(const QString &panelId) const;
    Q_INVOKABLE QVariantList panelIds() const;
    Q_INVOKABLE bool configurePanel(const QString &panelId, const QString &field,
                                    const QVariant &value);
    Q_INVOKABLE bool addPanel(const QString &edge);
    Q_INVOKABLE bool removePanel(const QString &panelId);
    // -- session --
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE void enterEditMode();
    Q_INVOKABLE void exitEditMode();
    Q_INVOKABLE void toggleEditMode();
    Q_INVOKABLE bool openCustomize();
    Q_INVOKABLE bool openWallpaperSettings();
    // -- edit-mode drags (arm + begin, hover, drop + apply, cancel) --
    Q_INVOKABLE bool beginAppletDrag(const QString &panelId, const QString &appletId);
    Q_INVOKABLE bool hoverDropTarget(const QString &panelId, const QString &zone,
                                     const QString &beforeAppletId);
    Q_INVOKABLE bool dropApplet();
    Q_INVOKABLE bool cancelDrag();
    // Solved surface geometry for cross-panel drops: the surface under a
    // global point on one output, and one surface by identity.
    Q_INVOKABLE QVariantMap panelSurfaceAt(const QString &outputId, double x, double y) const;
    Q_INVOKABLE QVariantMap panelSurface(const QString &outputId, const QString &panelId) const;

Q_SIGNALS:
    void changed();
    void editModeChanged();
    void chordChanged();
    // One line per menu action for evidence and logging: ok, action, message.
    void actionReported(bool ok, const QString &action, const QString &message);

private:
    [[nodiscard]] bool ensureHost();
    void rebuildHost();
    [[nodiscard]] bool settle(const QString &action,
                              const ShellCustomizationEditor::EditorOutcome &outcome,
                              bool applyAfter);
    [[nodiscard]] const Profiles::PanelSpec *panel(const QString &panelId) const;
    [[nodiscard]] const Profiles::AppletSpec *ownedApplet(const QString &ownerId,
                                                          const QString &appletId) const;
    void applyChordSnapshot();

    const QVector<Applets::AppletManifest> &m_manifests;
    QString m_userProfileDirectory;
    OutputSource m_outputs;
    Services::SettingsClient::SettingsClient *m_settings = nullptr;
    SettingsRouteLauncher *m_routes = nullptr;
    std::unique_ptr<ShellCustomizationEditor::LiveEditorHost> m_host;
    Profiles::LayoutProfile m_adopted;
    bool m_hasAdopted = false;
    bool m_outputsStale = false;
    bool m_editMode = false;
    bool m_dropAccepted = false;
    QString m_dropReason;
    QString m_statusText;
    QString m_chord;
};

} // namespace QindaQt::Shell
