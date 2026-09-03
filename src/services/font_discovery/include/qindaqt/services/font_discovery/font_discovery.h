// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/font_preferences/font_fact.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace QindaQt::Services::FontDiscovery {

using QindaQt::Services::FontPreferences::FontFact;

// AGENT-CONTRACT: Discovery bounds are part of the Font F1 provider contract
// (docs/wiki/architecture/font-preferences.md). A discovery run never retains
// more than maximumFacts facts and never retains a string field longer than
// maximumStringBytes UTF-8 bytes; over-long strings reject the whole pattern
// fail-closed rather than truncating into a different family identity.
struct FontDiscoveryLimits final {
    int maximumFacts = 4096;
    int maximumStringBytes = 512;

    [[nodiscard]] bool isValid() const noexcept
    {
        return maximumFacts > 0 && maximumFacts <= 65'536 && maximumStringBytes > 0
               && maximumStringBytes <= 16'384;
    }
};

// AGENT-CONTRACT: The provider builds its fontconfig FcConfig internally from
// exactly these injected inputs; the FcConfig never escapes the module. Tests
// always inject an explicit configurationFile and fontDirectories so the
// host's default fontconfig configuration and host font directories are never
// consulted. Only the exact productionDefault() shape (empty configuration
// file AND no injected directories) may resolve the default fontconfig
// configuration; a request with injected directories but no configuration
// file is ill-formed and rejected fail-closed (review finding P1-2).
struct FontDiscoveryRequest final {
    QStringList fontDirectories;
    QString configurationFile;
    FontDiscoveryLimits limits;

    [[nodiscard]] bool isWellFormed() const noexcept;

    // Default fontconfig configuration and no injected directories. This is
    // the only request shape allowed to touch host configuration.
    [[nodiscard]] static FontDiscoveryRequest productionDefault()
    {
        return FontDiscoveryRequest{};
    }
};

// AGENT-GUARD: available==false is the only unavailable truth; facts are then
// always empty. truncated is true when deterministic ordering kept exactly the
// first maximumFacts facts and dropped the rest.
struct FontDiscoveryResult final {
    bool available = false;
    bool truncated = false;
    QString diagnostic;
    QList<FontFact> facts;
};

// AGENT-NOTE: FontDiscoveryProvider is the single fontconfig-backed producer
// of F0 FontFact values (ADR-0067). It is the only module that includes
// fontconfig headers or links fontconfig; consumers receive plain value types.
class FontDiscoveryProvider final {
public:
    explicit FontDiscoveryProvider(FontDiscoveryRequest request);

    [[nodiscard]] const FontDiscoveryRequest &request() const noexcept { return m_request; }

    // AGENT-GUARD: discover() is fail-closed. Malformed requests, config parse
    // failures, missing injected directories, and fontconfig initialization
    // failures all return available==false with a bounded diagnostic and no
    // facts; the host font configuration is never mutated.
    [[nodiscard]] FontDiscoveryResult discover() const;

private:
    FontDiscoveryRequest m_request;
};

} // namespace QindaQt::Services::FontDiscovery
