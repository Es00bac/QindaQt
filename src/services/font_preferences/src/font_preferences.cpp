// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_preferences.h"

namespace QindaQt::Services::FontPreferences {

FontPreferences FontPreferences::systemDefaults() noexcept
{
    return FontPreferences{};
}

void FontPreferences::setFamily(const QString &family)
{
    m_family = normalizeFamilyName(family);
}

void FontPreferences::setMonospaceFamily(const QString &monospaceFamily)
{
    m_monospaceFamily = normalizeFamilyName(monospaceFamily);
}

void FontPreferences::setPointSize(double pointSize) noexcept
{
    m_pointSize = clampPointSize(pointSize);
}

void FontPreferences::setLogicalDpi(std::optional<double> dpi) noexcept
{
    if (dpi.has_value()) {
        m_logicalDpi = clampLogicalDpi(*dpi);
    } else {
        m_logicalDpi = std::nullopt;
    }
}

bool FontPreferences::isValid() const noexcept
{
    if (!isValidFamilyName(m_family) || !isValidFamilyName(m_monospaceFamily)) {
        return false;
    }
    if (!isValidPointSize(m_pointSize)) {
        return false;
    }
    if (m_logicalDpi.has_value() && !isValidLogicalDpi(*m_logicalDpi)) {
        return false;
    }
    return true;
}

FontPreferences FontPreferences::normalized() const
{
    FontPreferences result;
    result.setFamily(isValidFamilyName(m_family) ? m_family : QStringLiteral("Noto Sans"));
    result.setMonospaceFamily(isValidFamilyName(m_monospaceFamily) ? m_monospaceFamily : QStringLiteral("Noto Sans Mono"));
    result.setPointSize(clampPointSize(m_pointSize));
    result.setAntialiasing(m_antialiasing);
    result.setHinting(m_hinting);
    result.setSubpixelOrder(m_subpixelOrder);
    result.setLogicalDpi(m_logicalDpi.has_value() ? std::optional<double>(clampLogicalDpi(*m_logicalDpi)) : std::nullopt);
    return result;
}

bool FontPreferences::operator==(const FontPreferences &other) const noexcept
{
    return m_family == other.m_family &&
           m_monospaceFamily == other.m_monospaceFamily &&
           qFuzzyCompare(m_pointSize, other.m_pointSize) &&
           m_antialiasing == other.m_antialiasing &&
           m_hinting == other.m_hinting &&
           m_subpixelOrder == other.m_subpixelOrder &&
           ((!m_logicalDpi && !other.m_logicalDpi) ||
            (m_logicalDpi && other.m_logicalDpi && qFuzzyCompare(*m_logicalDpi, *other.m_logicalDpi)));
}

} // namespace QindaQt::Services::FontPreferences
