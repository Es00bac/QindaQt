// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QtGlobal>

namespace QindaQt::Services::FontPreferences {

inline constexpr double MinPointSize = 4.0;
inline constexpr double MaxPointSize = 144.0;
inline constexpr double DefaultPointSize = 10.0;

inline constexpr double MinLogicalDpi = 48.0;
inline constexpr double MaxLogicalDpi = 576.0;
inline constexpr double DefaultLogicalDpi = 96.0;

[[nodiscard]] bool isValidPointSize(double pointSize) noexcept;
[[nodiscard]] double clampPointSize(double pointSize) noexcept;

[[nodiscard]] bool isValidLogicalDpi(double dpi) noexcept;
[[nodiscard]] double clampLogicalDpi(double dpi) noexcept;

[[nodiscard]] bool isValidFamilyName(const QString &name) noexcept;
[[nodiscard]] QString normalizeFamilyName(const QString &name);

} // namespace QindaQt::Services::FontPreferences
