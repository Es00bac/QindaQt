// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

// Pure helpers behind the live customization menus: palette admission, typed
// applet-setting rows, and the drop-target arithmetic that turns "Move to end"
// or "Move left" into the one anchor the editor's MoveApplet intent needs.
// Nothing here touches the engine, QML, or Settings1; everything is testable
// against a LayoutProfile value.
namespace QindaQt::Shell::LiveCustomizationModel {

// Manifests admitted to a panel: at least one panel zone and the panel's
// orientation (the Settings route's keyboardInsertDefault rule).
[[nodiscard]] QVariantList paletteRows(const QVector<Applets::AppletManifest> &manifests,
                                       const Profiles::PanelSpec &panel);

// Typed rows for the applet menu's settings submenu. Only the three kinds the
// Settings route renders a real editor for are offered (boolean, bounded
// integer, closed string choice); everything else is left out rather than
// guessed at. Each row carries the effective value (stored or manifest default).
[[nodiscard]] QVariantList appletSettingRows(const Applets::AppletManifest &manifest,
                                             const QVariantMap &storedSettings);

struct SettingValidation final {
    QVariant value;
    QString error;
    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

// Mirrors the Settings route's validateAppletSettingValue contract: one typed
// value to store, or an error; never a partially accepted payload.
[[nodiscard]] SettingValidation validateAppletSetting(const QJsonObject &settingsSchema,
                                                      const QString &key,
                                                      const QVariant &value);

[[nodiscard]] QVariantMap panelOptions(const Profiles::PanelSpec &panel);

struct ZoneMove final {
    ShellCustomizationEditor::DropTarget target;
    // True when the applet already sits where the move would put it in the
    // panel's flat list, so only its zone setting needs to change.
    bool flatOrderUnchanged = false;
};

// "Move to <zone>": append after the zone's last applet.
[[nodiscard]] std::optional<ZoneMove> zoneMove(const Profiles::PanelSpec &panel,
                                               const QString &appletId,
                                               const QString &zone);
// "Move left/right": swap with the zone neighbour in reading order.
[[nodiscard]] std::optional<ShellCustomizationEditor::DropTarget>
stepMove(const Profiles::PanelSpec &panel, const QString &appletId, int delta);
// "Move to <panel>": append to the target panel keeping the applet's zone.
[[nodiscard]] std::optional<ShellCustomizationEditor::DropTarget>
panelMove(const Profiles::LayoutProfile &profile, const QString &sourcePanelId,
          const QString &appletId, const QString &targetPanelId);

[[nodiscard]] const Profiles::PanelSpec *findPanel(const Profiles::LayoutProfile &profile,
                                                   const QString &panelId);
[[nodiscard]] const Profiles::AppletSpec *findApplet(const Profiles::PanelSpec &panel,
                                                     const QString &appletId);
[[nodiscard]] const Applets::AppletManifest *findManifest(
    const QVector<Applets::AppletManifest> &manifests, const QString &pluginId);
[[nodiscard]] QString appletZone(const Profiles::AppletSpec &applet);

// Identity for new instances and panels: the first free "<plugin>-instance-N"
// / "panel-N" across the whole profile.
[[nodiscard]] QString nextInstanceId(const Profiles::LayoutProfile &profile,
                                     const QString &pluginId);
[[nodiscard]] QString nextPanelId(const Profiles::LayoutProfile &profile);
[[nodiscard]] Profiles::PanelSpec defaultPanel(const QString &id, Profiles::Edge edge);

// The zone a pointer fraction along a panel's main axis maps to (thirds), for
// cross-panel edit-mode drops where the target panel's real zone budgets are
// not known to the dragging window.
[[nodiscard]] QString zoneAtFraction(double fraction);

} // namespace QindaQt::Shell::LiveCustomizationModel
