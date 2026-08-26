// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_constraints/constraint_solution.h"
#include "qindaqt/hybrid_constraints/member_size_constraints.h"

#include "layoutnode.h"

#include <QHash>
#include <QRect>
#include <QString>

#include <optional>

namespace QindaQt::HybridConstraints {

class ConstraintSolver final
{
public:
    // This stateless operation retains no references and is safe to call from
    // any thread that owns its input values. Missing member entries are
    // unbounded; extra entries are ignored to allow registry-wide snapshots.
    // Invalid trees, metrics, constraints, or frames fail without a partial
    // solution. A valid result can still report recoverable size overflow.
    [[nodiscard]] static std::optional<ConstraintSolution> solve(
        const Core::LayoutNode &root,
        const QRect &outerFrame,
        const QHash<QString, MemberSizeConstraints> &memberConstraints,
        const LayoutMetrics &metrics,
        QString *error = nullptr);
};

} // namespace QindaQt::HybridConstraints
