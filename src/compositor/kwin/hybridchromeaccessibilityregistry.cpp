// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromeaccessibilityregistry.h"

#include "hybridchromeaccessibility_p.h"

#include <QSet>

#include <map>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

struct PreparedRoot final
{
    QString containerId;
    HybridChrome::ChromeRenderPlan plan;
    QMap<QString, QString> representatives;
    bool visible = false;
};

} // namespace

class HybridChromeAccessibilityRegistry::Private final
{
public:
    explicit Private(HybridChromeAccessibleActions accessibleActions)
        : actions(std::move(accessibleActions))
    {
    }

    HybridChromeAccessibleActions actions;
    std::map<QString, std::unique_ptr<HybridChromeAccessibilityAdapter>> adapters;
};

HybridChromeAccessibilityRegistry::HybridChromeAccessibilityRegistry(
    HybridChromeAccessibleActions actions)
    : d(std::make_unique<Private>(std::move(actions)))
{
}

HybridChromeAccessibilityRegistry::~HybridChromeAccessibilityRegistry() = default;

bool HybridChromeAccessibilityRegistry::synchronize(
    const QStringList &containerIds,
    const PlanLookup &planForContainer,
    const TabRepresentativeLookup &representativesForContainer,
    const VisibilityLookup &visibilityForContainer,
    QString *error)
{
    if (error) {
        error->clear();
    }
    if (!planForContainer || !representativesForContainer
        || !visibilityForContainer) {
        return fail(error, QStringLiteral("accessible chrome lookups are unavailable"));
    }

    QSet<QString> requested;
    QVector<PreparedRoot> prepared;
    prepared.reserve(containerIds.size());
    for (const auto &containerId : containerIds) {
        if (containerId.isEmpty() || requested.contains(containerId)) {
            return fail(error, QStringLiteral("accessible chrome has an invalid container set"));
        }
        requested.insert(containerId);
        auto plan = planForContainer(containerId);
        if (!plan || plan->containerId != containerId) {
            return fail(error, QStringLiteral("accessible chrome plan is stale for '%1'")
                                   .arg(containerId));
        }
        auto representatives = representativesForContainer(containerId);
        const bool visible = visibilityForContainer(containerId);
        QString planError;
        if (AccessibilityInternal::buildNodeSpecs(
                *plan, representatives, bool(d->actions.dispatch), visible,
                &planError).isEmpty()) {
            return fail(error, QStringLiteral("accessible chrome plan '%1' failed: %2")
                                   .arg(containerId, planError));
        }
        prepared.append({containerId, std::move(*plan),
                         std::move(representatives), visible});
    }

    for (const auto &root : prepared) {
        auto [iterator, inserted] = d->adapters.try_emplace(root.containerId);
        if (inserted) {
            iterator->second = std::make_unique<HybridChromeAccessibilityAdapter>(
                d->actions);
        }
        QString updateError;
        if (!iterator->second->updatePlan(
                root.plan, root.representatives, root.visible, &updateError)) {
            // AGENT-GUARD: Preflight used the same model builder, so a normal
            // update cannot fail here. Keep the root alive and report loudly
            // if future validation paths diverge rather than deleting state.
            return fail(error, QStringLiteral("accessible chrome publish '%1' failed: %2")
                                   .arg(root.containerId, updateError));
        }
    }
    for (auto iterator = d->adapters.begin(); iterator != d->adapters.end();) {
        if (!requested.contains(iterator->first)) {
            iterator = d->adapters.erase(iterator);
        } else {
            ++iterator;
        }
    }
    return true;
}

void HybridChromeAccessibilityRegistry::clear() noexcept
{
    d->adapters.clear();
}

QStringList HybridChromeAccessibilityRegistry::containerIds() const
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(d->adapters.size()));
    for (const auto &[containerId, adapter] : d->adapters) {
        Q_UNUSED(adapter)
        result.append(containerId);
    }
    return result;
}

HybridChromeAccessibilityAdapter *HybridChromeAccessibilityRegistry::adapter(
    const QString &containerId) const noexcept
{
    const auto found = d->adapters.find(containerId);
    return found == d->adapters.end() ? nullptr : found->second.get();
}

} // namespace QindaQt::Compositor::KWinIntegration
