// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QFlags>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace QindaQt::HybridConstraints {

enum class MaximizeAxis : quint8 {
    Horizontal = 0x1,
    Vertical = 0x2,
};
Q_DECLARE_FLAGS(MaximizeAxes, MaximizeAxis)

enum class QuickTileEdge : quint8 {
    Left = 0x1,
    Right = 0x2,
    Top = 0x4,
    Bottom = 0x8,
};
Q_DECLARE_FLAGS(QuickTileEdges, QuickTileEdge)

struct WindowRestoreState final
{
    static constexpr int JsonSchemaVersion = 2;

    // The independent frame is applied before restoring modes such as
    // maximize, quick tile, and fullscreen. IDs remain platform-neutral stable
    // handles; the adapter resolves them to live output/workspace objects.
    QRectF geometry;
    bool minimized = false;
    MaximizeAxes maximizedAxes;
    QuickTileEdges quickTileEdges;
    bool fullscreen = false;
    QString outputId;
    QStringList desktopIds;
    QStringList activityIds;
    bool keepAbove = false;
    bool keepBelow = false;
    bool focused = false;
    bool skipTaskbar = false;
    bool skipSwitcher = false;

    [[nodiscard]] bool isMaximized() const noexcept { return bool(maximizedAxes); }
    [[nodiscard]] bool isQuickTiled() const noexcept { return bool(quickTileEdges); }
    [[nodiscard]] bool isValid(QString *error = nullptr) const;

    // AGENT-CONTRACT: The compositor adapter owns the external window ID plus
    // capture/apply ordering. JSON schema v2 adds the two independent task-list
    // visibility bits; readers retain v1 compatibility by defaulting both to
    // false. This remains a process-neutral transaction value, not permission
    // to persist ephemeral focus across login sessions.
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<WindowRestoreState> fromJson(
        const QJsonObject &object,
        QString *error = nullptr);

    friend bool operator==(const WindowRestoreState &, const WindowRestoreState &) = default;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MaximizeAxes)
Q_DECLARE_OPERATORS_FOR_FLAGS(QuickTileEdges)

} // namespace QindaQt::HybridConstraints
