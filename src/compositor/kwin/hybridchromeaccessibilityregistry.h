// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridchromeaccessibility.h"

#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// Owns one virtual accessibility root per live collapsed container. Lookup
// callbacks are borrowed only for synchronize(); values are copied before any
// tree is mutated, so a chrome reconciliation cannot leave retained plan data.
class HybridChromeAccessibilityRegistry final
{
public:
    using PlanLookup = std::function<std::optional<HybridChrome::ChromeRenderPlan>(
        const QString &containerId)>;
    using TabRepresentativeLookup = std::function<QMap<QString, QString>(
        const QString &containerId)>;
    using VisibilityLookup = std::function<bool(const QString &containerId)>;

    explicit HybridChromeAccessibilityRegistry(
        HybridChromeAccessibleActions actions = {});
    ~HybridChromeAccessibilityRegistry();

    HybridChromeAccessibilityRegistry(
        const HybridChromeAccessibilityRegistry &) = delete;
    HybridChromeAccessibilityRegistry &operator=(
        const HybridChromeAccessibilityRegistry &) = delete;

    // Preflights the entire borrowed snapshot before publishing any node
    // changes. containerIds is the authoritative lifetime set; roots absent
    // from it are hidden and destroyed only after all current roots update.
    [[nodiscard]] bool synchronize(
        const QStringList &containerIds,
        const PlanLookup &planForContainer,
        const TabRepresentativeLookup &representativesForContainer,
        const VisibilityLookup &visibilityForContainer,
        QString *error = nullptr);
    void clear() noexcept;

    [[nodiscard]] QStringList containerIds() const;
    [[nodiscard]] HybridChromeAccessibilityAdapter *adapter(
        const QString &containerId) const noexcept;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Compositor::KWinIntegration
