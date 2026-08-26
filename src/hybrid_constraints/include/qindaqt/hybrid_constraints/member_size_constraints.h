// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QMargins>
#include <QSize>
#include <QString>

#include <optional>

namespace QindaQt::HybridConstraints {

struct MemberSizeConstraints final
{
    QSize minimumSize{0, 0};
    std::optional<QSize> maximumSize;
    std::optional<QSize> fixedSize;

    [[nodiscard]] bool isValid(QString *error = nullptr) const;
    [[nodiscard]] QSize effectiveMinimumSize() const noexcept;
    [[nodiscard]] std::optional<QSize> effectiveMaximumSize() const noexcept;

    friend bool operator==(const MemberSizeConstraints &,
                           const MemberSizeConstraints &) = default;
};

struct LayoutMetrics final
{
    // Insets consume the shared container chrome before page layout begins.
    QMargins contentInsets;
    int dividerThickness = 0;

    [[nodiscard]] bool isValid(QString *error = nullptr) const;

    friend bool operator==(const LayoutMetrics &, const LayoutMetrics &) = default;
};

} // namespace QindaQt::HybridConstraints
