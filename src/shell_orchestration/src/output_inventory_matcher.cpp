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

} // namespace

OutputInventoryMatchResult OutputInventoryMatcher::match(
    const QVector<ShellLayout::LogicalOutput> &expected,
    const QVector<ShellLayout::LogicalOutput> &observed)
{
    if (expected.isEmpty() || observed.isEmpty()) {
        return failure(OutputInventoryMatchErrorCode::EmptyInventory, {},
                       QStringLiteral("output inventories must not be empty"));
    }
    if (expected.size() != observed.size()) {
        return failure(OutputInventoryMatchErrorCode::CountMismatch, {},
                       QStringLiteral("output inventory counts differ"));
    }
    QHash<QString, const ShellLayout::LogicalOutput *> expectedById;
    QHash<QString, const ShellLayout::LogicalOutput *> observedById;
    if (const auto result = validate(expected, &expectedById); !result.ok()) {
        return result;
    }
    if (const auto result = validate(observed, &observedById); !result.ok()) {
        return result;
    }
    for (auto item = expectedById.cbegin(); item != expectedById.cend(); ++item) {
        const auto candidate = observedById.constFind(item.key());
        if (candidate == observedById.cend()) {
            return failure(OutputInventoryMatchErrorCode::MissingOutput, item.key(),
                           QStringLiteral("output '%1' is missing").arg(item.key()));
        }
        if (item.value()->geometry != candidate.value()->geometry) {
            return failure(OutputInventoryMatchErrorCode::GeometryMismatch, item.key(),
                           QStringLiteral("output '%1' logical geometry differs")
                               .arg(item.key()));
        }
        if (item.value()->scale != candidate.value()->scale) {
            return failure(OutputInventoryMatchErrorCode::ScaleMismatch, item.key(),
                           QStringLiteral("output '%1' scale differs").arg(item.key()));
        }
    }
    return {};
}

} // namespace QindaQt::ShellOrchestration
