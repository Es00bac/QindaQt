// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_constraints/member_size_constraints.h"

namespace QindaQt::HybridConstraints {
namespace {

bool hasNonNegativeDimensions(const QSize &size) noexcept
{
    return size.width() >= 0 && size.height() >= 0;
}

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool MemberSizeConstraints::isValid(QString *error) const
{
    if (!hasNonNegativeDimensions(minimumSize)) {
        setError(error, QStringLiteral("minimum size must have non-negative dimensions"));
        return false;
    }
    if (maximumSize.has_value()
        && (!hasNonNegativeDimensions(*maximumSize)
            || maximumSize->width() < minimumSize.width()
            || maximumSize->height() < minimumSize.height())) {
        setError(error,
                 QStringLiteral("maximum size must be non-negative and no smaller than minimum"));
        return false;
    }
    if (fixedSize.has_value() && !hasNonNegativeDimensions(*fixedSize)) {
        setError(error, QStringLiteral("fixed size must have non-negative dimensions"));
        return false;
    }
    if (fixedSize.has_value()
        && (fixedSize->width() < minimumSize.width()
            || fixedSize->height() < minimumSize.height())) {
        setError(error, QStringLiteral("fixed size must satisfy the declared minimum size"));
        return false;
    }
    if (fixedSize.has_value() && maximumSize.has_value()
        && (fixedSize->width() > maximumSize->width()
            || fixedSize->height() > maximumSize->height())) {
        setError(error, QStringLiteral("fixed size must satisfy the declared maximum size"));
        return false;
    }
    return true;
}

QSize MemberSizeConstraints::effectiveMinimumSize() const noexcept
{
    return fixedSize.value_or(minimumSize);
}

std::optional<QSize> MemberSizeConstraints::effectiveMaximumSize() const noexcept
{
    if (fixedSize.has_value()) {
        return fixedSize;
    }
    return maximumSize;
}

bool LayoutMetrics::isValid(QString *error) const
{
    if (contentInsets.left() < 0 || contentInsets.top() < 0
        || contentInsets.right() < 0 || contentInsets.bottom() < 0) {
        setError(error, QStringLiteral("content insets must be non-negative"));
        return false;
    }
    if (dividerThickness < 0) {
        setError(error, QStringLiteral("divider thickness must be non-negative"));
        return false;
    }
    return true;
}

} // namespace QindaQt::HybridConstraints
