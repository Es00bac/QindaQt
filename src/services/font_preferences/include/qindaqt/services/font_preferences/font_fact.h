// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <tuple>

namespace QindaQt::Services::FontPreferences {

// AGENT-NOTE: FontFact represents an atomic injected fact about an available
// font file or fontconfig pattern, avoiding direct host filesystem access.
struct FontFact final {
    QString family;
    QString style = QStringLiteral("Regular");
    bool isMonospace = false;
    bool isScalable = true;
    int weight = 400;
    bool italic = false;
    QString postscriptName;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (family.trimmed().isEmpty()) {
            return false;
        }
        for (const auto &ch : family) {
            if (ch.isNull() || ((ch.isLowSurrogate() == ch.isHighSurrogate()) && ch.category() == QChar::Other_Control)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool operator==(const FontFact &other) const noexcept
    {
        return family == other.family &&
               style == other.style &&
               isMonospace == other.isMonospace &&
               isScalable == other.isScalable &&
               weight == other.weight &&
               italic == other.italic &&
               postscriptName == other.postscriptName;
    }

    [[nodiscard]] bool operator<(const FontFact &other) const noexcept
    {
        return std::tie(family, style, isMonospace, isScalable, weight, italic, postscriptName) <
               std::tie(other.family, other.style, other.isMonospace, other.isScalable, other.weight, other.italic, other.postscriptName);
    }
};

} // namespace QindaQt::Services::FontPreferences
