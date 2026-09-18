// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

#include <optional>

namespace QindaQt::Apps::SettingsWindows {

// AGENT-CONTRACT: The route scopes its Settings1 client to exactly these
// four schema v2 keys, the ones the session bridge (ADR-0199) carries into
// kwinrc and the compositor consumes live. The schema's session-restore key
// exists but has no consumer yet, so it is deliberately not scoped, read, or
// written here; the boundary scan rejects any route source naming it
// (see docs/wiki/apps/windows-settings.md).
struct WindowsKeys final {
    static constexpr char FocusPolicy[] = "windowManagement.focusPolicy";
    static constexpr char DockingModifier[] = "windowManagement.dockingModifier";
    static constexpr char SnapDistance[] = "windowManagement.snapDistance";
    static constexpr char CloseContainerPolicy[] = "windowManagement.closeContainerPolicy";

    // Deterministic commit order; applyDraft() writes changed keys in this order.
    [[nodiscard]] static QStringList scopedKeys();
};

// Typed projection of the four scoped values. Tokens and bounds mirror the
// schema v2 constraints exactly; the route never writes a value the service
// would reject.
struct WindowsValues final {
    static constexpr int DefaultSnapDistance = 12;
    static constexpr int MinimumSnapDistance = 0;
    static constexpr int MaximumSnapDistance = 64;

    QString focusPolicy = QStringLiteral("click");
    QString dockingModifier = QStringLiteral("super");
    int snapDistance = DefaultSnapDistance;
    QString closeContainerPolicy = QStringLiteral("ask");

    [[nodiscard]] static QStringList focusPolicyTokens();
    [[nodiscard]] static QStringList dockingModifierTokens();
    [[nodiscard]] static QStringList closeContainerPolicyTokens();
    [[nodiscard]] static bool isValidSnapDistance(double distance) noexcept;
    // Fails closed: every scoped key must be present with a known token or an
    // in-range integral number.
    [[nodiscard]] static std::optional<WindowsValues>
    fromVariantMap(const QVariantMap &values, QString *error = nullptr);
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QVariant value(const QString &key) const;
    // True when `wire` (a service-reported value) denotes the same value as
    // `intended` for `key`, tolerating floating wire numbers for the distance.
    [[nodiscard]] static bool sameValue(const QString &key, const QVariant &wire,
                                        const QVariant &intended);
    [[nodiscard]] bool operator==(const WindowsValues &other) const noexcept;
    [[nodiscard]] bool operator!=(const WindowsValues &other) const noexcept
    {
        return !(*this == other);
    }
};

} // namespace QindaQt::Apps::SettingsWindows
