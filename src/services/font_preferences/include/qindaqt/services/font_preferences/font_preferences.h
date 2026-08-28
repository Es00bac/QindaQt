// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_types.h"
#include "qindaqt/services/font_preferences/font_validation.h"

#include <QString>
#include <optional>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontPreferences holds validated typography preferences
// matching both Settings1 schema keys and QST-1 design token consumption.
class FontPreferences final {
public:
    FontPreferences() = default;

    [[nodiscard]] static FontPreferences systemDefaults() noexcept;

    [[nodiscard]] const QString &family() const noexcept { return m_family; }
    void setFamily(const QString &family);

    [[nodiscard]] const QString &monospaceFamily() const noexcept { return m_monospaceFamily; }
    void setMonospaceFamily(const QString &monospaceFamily);

    [[nodiscard]] double pointSize() const noexcept { return m_pointSize; }
    void setPointSize(double pointSize) noexcept;

    [[nodiscard]] FontAntialiasing antialiasing() const noexcept { return m_antialiasing; }
    void setAntialiasing(FontAntialiasing antialiasing) noexcept { m_antialiasing = antialiasing; }

    [[nodiscard]] FontHinting hinting() const noexcept { return m_hinting; }
    void setHinting(FontHinting hinting) noexcept { m_hinting = hinting; }

    [[nodiscard]] FontSubpixelOrder subpixelOrder() const noexcept { return m_subpixelOrder; }
    void setSubpixelOrder(FontSubpixelOrder subpixelOrder) noexcept { m_subpixelOrder = subpixelOrder; }

    [[nodiscard]] std::optional<double> logicalDpi() const noexcept { return m_logicalDpi; }
    void setLogicalDpi(std::optional<double> dpi) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] FontPreferences normalized() const;

    [[nodiscard]] bool operator==(const FontPreferences &other) const noexcept;

private:
    QString m_family = QStringLiteral("Noto Sans");
    QString m_monospaceFamily = QStringLiteral("Noto Sans Mono");
    double m_pointSize = DefaultPointSize;
    FontAntialiasing m_antialiasing = FontAntialiasing::Subpixel;
    FontHinting m_hinting = FontHinting::Slight;
    FontSubpixelOrder m_subpixelOrder = FontSubpixelOrder::Rgb;
    std::optional<double> m_logicalDpi = std::nullopt;
};

} // namespace QindaQt::Services::FontPreferences
