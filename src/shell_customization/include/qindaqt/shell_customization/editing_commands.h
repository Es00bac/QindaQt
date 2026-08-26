// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QString>
#include <QVariantMap>
#include <QtTypes>

#include <optional>
#include <variant>

namespace QindaQt::ShellCustomization {

enum class EditingCommandKind {
    AddPanel,
    RemovePanel,
    MovePanel,
    ConfigurePanel,
    InsertApplet,
    MoveApplet,
    RemoveApplet,
    DuplicateApplet,
    UpdateAppletSettings,
    Undo,
    Redo,
    BeginPreview,
    CommitPreview,
    CancelPreview,
};

struct AddPanelCommand final {
    quint64 expectedRevision = 0;
    Profiles::PanelSpec panel;
    std::optional<QString> beforePanelId;
};

struct RemovePanelCommand final {
    quint64 expectedRevision = 0;
    QString panelId;
};

struct MovePanelCommand final {
    quint64 expectedRevision = 0;
    QString panelId;
    QString outputId;
    Profiles::Edge edge = Profiles::Edge::Top;
    Profiles::Alignment alignment = Profiles::Alignment::Fill;
    std::optional<QString> beforePanelId;
};

struct ConfigurePanelCommand final {
    quint64 expectedRevision = 0;
    QString panelId;
    Profiles::Layer layer = Profiles::Layer::Above;
    Profiles::HideMode hideMode = Profiles::HideMode::Never;
    int rows = 1;
    int thickness = 32;
    double length = 1.0;
};

struct InsertAppletCommand final {
    quint64 expectedRevision = 0;
    QString panelId;
    QString instanceId;
    QString pluginId;
    QVariantMap initialSettings;
    std::optional<QString> beforeAppletId;
};

struct MoveAppletCommand final {
    quint64 expectedRevision = 0;
    QString sourcePanelId;
    QString appletId;
    QString targetPanelId;
    std::optional<QString> beforeAppletId;
};

struct RemoveAppletCommand final {
    quint64 expectedRevision = 0;
    QString panelId;
    QString appletId;
};

struct DuplicateAppletCommand final {
    quint64 expectedRevision = 0;
    QString sourcePanelId;
    QString appletId;
    QString targetPanelId;
    QString newAppletId;
    std::optional<QString> beforeAppletId;
};

struct UpdateAppletSettingsCommand final {
    quint64 expectedRevision = 0;
    QString panelId;
    QString appletId;
    QVariantMap settings;
};

struct UndoCommand final {
    quint64 expectedRevision = 0;
};

struct RedoCommand final {
    quint64 expectedRevision = 0;
};

struct BeginPreviewCommand final {
    quint64 expectedRevision = 0;
};

struct CommitPreviewCommand final {
    quint64 expectedRevision = 0;
};

struct CancelPreviewCommand final {
    quint64 expectedRevision = 0;
};

using EditingCommand = std::variant<AddPanelCommand,
                                    RemovePanelCommand,
                                    MovePanelCommand,
                                    ConfigurePanelCommand,
                                    InsertAppletCommand,
                                    MoveAppletCommand,
                                    RemoveAppletCommand,
                                    DuplicateAppletCommand,
                                    UpdateAppletSettingsCommand,
                                    UndoCommand,
                                    RedoCommand,
                                    BeginPreviewCommand,
                                    CommitPreviewCommand,
                                    CancelPreviewCommand>;

[[nodiscard]] EditingCommandKind commandKind(const EditingCommand &command) noexcept;
// Every command is optimistic. The coordinator rejects a mismatch before
// copying or validating a candidate and never advances the revision on error.
[[nodiscard]] quint64 expectedRevision(const EditingCommand &command) noexcept;

} // namespace QindaQt::ShellCustomization
