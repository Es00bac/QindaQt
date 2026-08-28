// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_validation.h"

#include <algorithm>
#include <cmath>

namespace QindaQt::Services::FontPreferences {

bool isValidPointSize(double pointSize) noexcept
{
    return std::isfinite(pointSize) && pointSize >= MinPointSize && pointSize <= MaxPointSize;
}

double clampPointSize(double pointSize) noexcept
{
    if (!std::isfinite(pointSize)) {
        return DefaultPointSize;
    }
    return std::clamp(pointSize, MinPointSize, MaxPointSize);
}

bool isValidLogicalDpi(double dpi) noexcept
{
    return std::isfinite(dpi) && dpi >= MinLogicalDpi && dpi <= MaxLogicalDpi;
}

double clampLogicalDpi(double dpi) noexcept
{
    if (!std::isfinite(dpi)) {
        return DefaultLogicalDpi;
    }
    return std::clamp(dpi, MinLogicalDpi, MaxLogicalDpi);
}

bool isValidFamilyName(const QString &name) noexcept
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    for (const auto &ch : trimmed) {
        if (ch.isNull() || ((ch.isLowSurrogate() == ch.isHighSurrogate()) && ch.category() == QChar::Other_Control)) {
            return false;
        }
    }
    return true;
}

QString normalizeFamilyName(const QString &name)
{
    QString result;
    result.reserve(name.size());
    bool inSpace = false;
    for (const auto &ch : name) {
        if (ch.isSpace()) {
            if (!inSpace && !result.isEmpty()) {
                result.append(QLatin1Char(' '));
                inSpace = true;
            }
        } else {
            result.append(ch);
            inSpace = false;
        }
    }
    while (result.endsWith(QLatin1Char(' '))) {
        result.chop(1);
    }
    return result;
}

} // namespace QindaQt::Services::FontPreferences
