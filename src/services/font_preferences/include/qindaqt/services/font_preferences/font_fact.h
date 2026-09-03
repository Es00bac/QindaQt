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

    // AGENT-GUARD (review finding P1-3): Every published string field -- not
    // only family -- must be free of NUL and control characters before a fact
    // reaches a consumer. A hostile fontconfig scan rule can inject arbitrary
    // bytes into FC_STYLE/FC_POSTSCRIPT_NAME; validating only family would
    // publish them.
    [[nodiscard]] bool isValid() const noexcept
    {
        if (family.trimmed().isEmpty()) {
            return false;
        }
        const auto freeOfControlCharacters = [](const QString &value) noexcept {
            for (const auto &ch : value) {
                if (ch.isNull() || ((ch.isLowSurrogate() == ch.isHighSurrogate()) && ch.category() == QChar::Other_Control)) {
                    return false;
                }
            }
            return true;
        };
        return freeOfControlCharacters(family) && freeOfControlCharacters(style)
               && freeOfControlCharacters(postscriptName);
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
