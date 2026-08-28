// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_fact.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: FontFamilyEntry represents a normalized, deduplicated family
// in the pure catalog, aggregating all discovered styles.
struct FontFamilyEntry final {
    QString familyName;
    QString canonicalKey;
    QStringList styles;
    bool isMonospace = false;
    bool isScalable = true;

    [[nodiscard]] bool operator==(const FontFamilyEntry &other) const noexcept
    {
        return familyName == other.familyName &&
               canonicalKey == other.canonicalKey &&
               styles == other.styles &&
               isMonospace == other.isMonospace &&
               isScalable == other.isScalable;
    }
};

// AGENT-GUARD: FontCatalog is pure and immutable once constructed. Facts with
// duplicate/unstable identity or invalid control characters are rejected.
class FontCatalog final {
public:
    FontCatalog() = default;
    explicit FontCatalog(QList<FontFamilyEntry> entries);

    [[nodiscard]] static FontCatalog create(
        const QList<FontFact> &facts,
        QString *errorMessage = nullptr);

    [[nodiscard]] static FontCatalog createDefaultFallback();

    // AGENT-GUARD: FontCatalog::create returns an empty (invalid) catalog if and only if
    // fact validation or catalog generation fails. Non-empty valid facts always produce
    // a non-empty valid catalog.
    [[nodiscard]] bool isValid() const noexcept { return !m_entries.isEmpty(); }
    [[nodiscard]] bool isEmpty() const noexcept { return m_entries.isEmpty(); }
    [[nodiscard]] int familyCount() const noexcept { return static_cast<int>(m_entries.size()); }
    [[nodiscard]] bool containsFamily(const QString &familyName) const noexcept;
    [[nodiscard]] std::optional<FontFamilyEntry> findFamily(const QString &familyName) const noexcept;
    [[nodiscard]] QStringList familyNames() const;
    [[nodiscard]] QStringList monospaceFamilyNames() const;
    [[nodiscard]] QStringList proportionalFamilyNames() const;
    [[nodiscard]] const QList<FontFamilyEntry> &entries() const noexcept { return m_entries; }

    [[nodiscard]] std::optional<QString> resolveFamily(
        const QString &requestedFamily,
        const QString &fallbackFamily = QString()) const;

    [[nodiscard]] bool operator==(const FontCatalog &other) const noexcept
    {
        return m_entries == other.m_entries;
    }

private:
    QList<FontFamilyEntry> m_entries;
};

} // namespace QindaQt::Services::FontPreferences
