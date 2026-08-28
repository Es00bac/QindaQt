// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_catalog.h"

#include <QMap>
#include <QSet>
#include <algorithm>

namespace QindaQt::Services::FontPreferences {

namespace {

QString normalizeName(const QString &raw)
{
    QString result;
    result.reserve(raw.size());
    bool inSpace = false;
    for (const auto &ch : raw) {
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

} // namespace

FontCatalog::FontCatalog(QList<FontFamilyEntry> entries)
    : m_entries(std::move(entries))
{
    std::sort(m_entries.begin(), m_entries.end(), [](const FontFamilyEntry &a, const FontFamilyEntry &b) {
        return a.canonicalKey < b.canonicalKey;
    });
}

// AGENT-GUARD: FontCatalog::create guarantees that a non-empty list of valid facts produces
// a valid catalog (isValid() == true, familyCount() > 0). An empty catalog is returned if and
// only if creation fails or facts are empty/invalid. FontPreferencesCoordinator relies on this
// explicit invariant for atomic LKG retention.
FontCatalog FontCatalog::create(const QList<FontFact> &facts, QString *errorMessage)
{
    if (facts.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Font facts list cannot be empty");
        }
        return {};
    }

    struct FamilyBuilder {
        QString displayName;
        QString canonicalKey;
        QSet<QString> styles;
        bool isMonospace = false;
        bool monospaceSet = false;
        bool isScalable = false;
    };

    QMap<QString, FamilyBuilder> builders;

    for (const auto &fact : facts) {
        if (!fact.isValid()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Invalid font fact detected: family '%1' is invalid or contains unprintable characters")
                                    .arg(fact.family);
            }
            return {};
        }

        const QString normalizedFamily = normalizeName(fact.family);
        if (normalizedFamily.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Font fact contains empty family name after normalization");
            }
            return {};
        }

        const QString canonical = normalizedFamily.toCaseFolded();
        auto &builder = builders[canonical];
        if (builder.canonicalKey.isEmpty()) {
            builder.displayName = normalizedFamily;
            builder.canonicalKey = canonical;
            builder.isMonospace = fact.isMonospace;
            builder.monospaceSet = true;
            builder.isScalable = fact.isScalable;
        } else {
            // AGENT-GUARD: Reject conflicting monospace designations for the same family identity
            if (builder.monospaceSet && builder.isMonospace != fact.isMonospace) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Conflicting monospace designation for family '%1'").arg(normalizedFamily);
                }
                return {};
            }
            builder.isScalable = builder.isScalable || fact.isScalable;
        }

        const QString normalizedStyle = fact.style.trimmed().isEmpty() ? QStringLiteral("Regular") : fact.style.trimmed();
        builder.styles.insert(normalizedStyle);
    }

    QList<FontFamilyEntry> entries;
    entries.reserve(builders.size());

    for (auto it = builders.cbegin(); it != builders.cend(); ++it) {
        const auto &b = it.value();
        FontFamilyEntry entry;
        entry.familyName = b.displayName;
        entry.canonicalKey = b.canonicalKey;
        entry.isMonospace = b.isMonospace;
        entry.isScalable = b.isScalable;
        entry.styles = b.styles.values();
        std::sort(entry.styles.begin(), entry.styles.end());
        entries.append(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const FontFamilyEntry &a, const FontFamilyEntry &b) {
        return a.canonicalKey < b.canonicalKey;
    });

    return FontCatalog(std::move(entries));
}

FontCatalog FontCatalog::createDefaultFallback()
{
    const QList<FontFact> fallbackFacts = {
        {QStringLiteral("Noto Sans"), QStringLiteral("Regular"), false, true, 400, false, QStringLiteral("NotoSans-Regular")},
        {QStringLiteral("Noto Sans"), QStringLiteral("Bold"), false, true, 700, false, QStringLiteral("NotoSans-Bold")},
        {QStringLiteral("Noto Sans Mono"), QStringLiteral("Regular"), true, true, 400, false, QStringLiteral("NotoSansMono-Regular")},
        {QStringLiteral("Noto Sans Mono"), QStringLiteral("Bold"), true, true, 700, false, QStringLiteral("NotoSansMono-Bold")},
        {QStringLiteral("Sans Serif"), QStringLiteral("Regular"), false, true, 400, false, QString()},
        {QStringLiteral("Monospace"), QStringLiteral("Regular"), true, true, 400, false, QString()}
    };
    return FontCatalog::create(fallbackFacts);
}

bool FontCatalog::containsFamily(const QString &familyName) const noexcept
{
    const QString canonical = normalizeName(familyName).toCaseFolded();
    return std::any_of(m_entries.begin(), m_entries.end(), [&canonical](const FontFamilyEntry &entry) {
        return entry.canonicalKey == canonical;
    });
}

std::optional<FontFamilyEntry> FontCatalog::findFamily(const QString &familyName) const noexcept
{
    const QString canonical = normalizeName(familyName).toCaseFolded();
    auto it = std::find_if(m_entries.begin(), m_entries.end(), [&canonical](const FontFamilyEntry &entry) {
        return entry.canonicalKey == canonical;
    });
    if (it != m_entries.end()) {
        return *it;
    }
    return std::nullopt;
}

QStringList FontCatalog::familyNames() const
{
    QStringList result;
    result.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        result.append(entry.familyName);
    }
    return result;
}

QStringList FontCatalog::monospaceFamilyNames() const
{
    QStringList result;
    for (const auto &entry : m_entries) {
        if (entry.isMonospace) {
            result.append(entry.familyName);
        }
    }
    return result;
}

QStringList FontCatalog::proportionalFamilyNames() const
{
    QStringList result;
    for (const auto &entry : m_entries) {
        if (!entry.isMonospace) {
            result.append(entry.familyName);
        }
    }
    return result;
}

std::optional<QString> FontCatalog::resolveFamily(
    const QString &requestedFamily,
    const QString &fallbackFamily) const
{
    if (const auto match = findFamily(requestedFamily)) {
        return match->familyName;
    }
    if (!fallbackFamily.isEmpty()) {
        if (const auto fallbackMatch = findFamily(fallbackFamily)) {
            return fallbackMatch->familyName;
        }
    }
    if (!m_entries.isEmpty()) {
        return m_entries.first().familyName;
    }
    return std::nullopt;
}

} // namespace QindaQt::Services::FontPreferences
