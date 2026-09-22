// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_orchestration/output_inventory_matcher.h"

#include <QHash>

#include <cmath>
#include <utility>

namespace QindaQt::ShellOrchestration {
namespace {

OutputInventoryMatchResult failure(OutputInventoryMatchErrorCode code,
                                   QString outputId, QString message)
{
    return {code, std::move(outputId), std::move(message)};
}

OutputInventoryMatchResult validate(
    const QVector<ShellLayout::LogicalOutput> &inventory,
    QHash<QString, const ShellLayout::LogicalOutput *> *byId)
{
    for (const auto &output : inventory) {
        if (output.id.trimmed().isEmpty() || !output.geometry.isValid() ||
            !std::isfinite(output.scale) || output.scale <= 0.0) {
            return failure(OutputInventoryMatchErrorCode::InvalidOutput, output.id,
                           QStringLiteral("output inventory contains an invalid output"));
        }
        if (byId->contains(output.id)) {
            return failure(OutputInventoryMatchErrorCode::DuplicateOutput, output.id,
                           QStringLiteral("output '%1' is duplicated").arg(output.id));
        }
        byId->insert(output.id, &output);
    }
    return {};
}

// AGENT-GUARD: the two inventories measure scale with different rulers and
// must never be compared for exact equality. `compositorScale` is the
// wl_output logical scale and may be fractional (1.25, 1.5, 1.75). `qtScale`
// is QScreen::devicePixelRatio(), which the Qt Wayland platform reports as the
// integer buffer scale for the output - the compositor scale rounded up.
// Demanding equality permanently rejected every fractionally scaled output and
// pinned the shell in the safe-visible fallback, so no panel could ever hide
// (review finding: "output 'eDP-1' scale differs" on a 1.25 output).
// Accept either the same ruler (integer scales, and platforms that report the
// fractional ratio directly) or Qt's documented integer envelope. A genuinely
// crossed output generation still fails, because no other value satisfies
// either relation.
bool scalesDescribeTheSameOutput(qreal compositorScale, qreal qtScale)
{
    constexpr qreal tolerance = 1.0 / 1024.0;
    if (std::abs(compositorScale - qtScale) <= tolerance) {
        return true;
    }
    return std::abs(std::ceil(compositorScale) - qtScale) <= tolerance;
}

} // namespace

OutputInventoryMatchResult OutputInventoryMatcher::match(
    const QVector<ShellLayout::LogicalOutput> &compositorInventory,
    const QVector<ShellLayout::LogicalOutput> &qtInventory)
{
    if (compositorInventory.isEmpty() || qtInventory.isEmpty()) {
        return failure(OutputInventoryMatchErrorCode::EmptyInventory, {},
                       QStringLiteral("output inventories must not be empty"));
    }
    if (compositorInventory.size() != qtInventory.size()) {
        return failure(OutputInventoryMatchErrorCode::CountMismatch, {},
                       QStringLiteral("output inventory counts differ"));
    }
    QHash<QString, const ShellLayout::LogicalOutput *> compositorById;
    QHash<QString, const ShellLayout::LogicalOutput *> qtById;
    if (const auto result = validate(compositorInventory, &compositorById); !result.ok()) {
        return result;
    }
    if (const auto result = validate(qtInventory, &qtById); !result.ok()) {
        return result;
    }
    for (auto item = compositorById.cbegin(); item != compositorById.cend(); ++item) {
        const auto candidate = qtById.constFind(item.key());
        if (candidate == qtById.cend()) {
            return failure(OutputInventoryMatchErrorCode::MissingOutput, item.key(),
                           QStringLiteral("output '%1' is missing").arg(item.key()));
        }
        if (item.value()->geometry != candidate.value()->geometry) {
            return failure(OutputInventoryMatchErrorCode::GeometryMismatch, item.key(),
                           QStringLiteral("output '%1' logical geometry differs")
                               .arg(item.key()));
        }
        if (!scalesDescribeTheSameOutput(item.value()->scale,
                                         candidate.value()->scale)) {
            return failure(OutputInventoryMatchErrorCode::ScaleMismatch, item.key(),
                           QStringLiteral("output '%1' scale differs").arg(item.key()));
        }
    }
    return {};
}

} // namespace QindaQt::ShellOrchestration
