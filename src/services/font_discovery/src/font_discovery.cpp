// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/font_discovery/font_discovery.h"

#include <QFileInfo>

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

namespace QindaQt::Services::FontDiscovery {

bool FontDiscoveryRequest::isWellFormed() const noexcept
{
    if (!limits.isValid()) {
        return false;
    }
    for (const QString &directory : fontDirectories) {
        if (directory.trimmed().isEmpty()) {
            return false;
        }
    }
    // AGENT-GUARD (review finding P1-2): Only the exact productionDefault()
    // shape -- no injected directories AND no configuration file -- may reach
    // FcInitLoadConfig(). An injected directory with an empty configuration
    // file would silently enumerate the host's ambient fontconfig state, so
    // the request is ill-formed and discovery fails closed instead.
    if (configurationFile.trimmed().isEmpty() && !fontDirectories.isEmpty()) {
        return false;
    }
    return true;
}

namespace {

// AGENT-NOTE: fontconfig weight uses its own 0..215 scale while FontFact
// carries the CSS/OpenType 100..950 scale (400 regular). The interpolation
// anchors mirror FcWeightToOpenType so discovered weights match what Qt
// reports for the same faces without depending on a fontconfig version that
// exports the conversion helper.
int weightToCssScale(int fontconfigWeight) noexcept
{
    struct Anchor {
        int fc;
        int css;
    };
    static constexpr std::array anchors{
        Anchor{0, 100},   Anchor{40, 200}, Anchor{50, 300},  Anchor{55, 350},
        Anchor{75, 380},  Anchor{80, 400}, Anchor{100, 500}, Anchor{180, 600},
        Anchor{200, 700}, Anchor{205, 800}, Anchor{210, 900}, Anchor{215, 950},
    };
    if (fontconfigWeight <= anchors.front().fc) {
        return anchors.front().css;
    }
    if (fontconfigWeight >= anchors.back().fc) {
        return anchors.back().css;
    }
    for (std::size_t i = 1; i < anchors.size(); ++i) {
        if (fontconfigWeight <= anchors[i].fc) {
            const auto &lo = anchors[i - 1];
            const auto &hi = anchors[i];
            const double t = static_cast<double>(fontconfigWeight - lo.fc)
                             / static_cast<double>(hi.fc - lo.fc);
            return lo.css + static_cast<int>(t * static_cast<double>(hi.css - lo.css) + 0.5);
        }
    }
    return 400;
}

struct ConfigDeleter {
    void operator()(FcConfig *config) const noexcept
    {
        if (config != nullptr) {
            FcConfigDestroy(config);
        }
    }
};
using ConfigHolder = std::unique_ptr<FcConfig, ConfigDeleter>;

void setDiagnostic(QString *output, const QString &message)
{
    if (output != nullptr) {
        *output = message;
    }
}

ConfigHolder buildConfig(const FontDiscoveryRequest &request, QString *diagnostic)
{
    if (request.configurationFile.isEmpty()) {
        // AGENT-CONTRACT: Only the production default request reaches this
        // branch; injected test requests always name an explicit file.
        FcConfig *config = FcInitLoadConfig();
        if (config == nullptr) {
            setDiagnostic(diagnostic, QStringLiteral("fontconfig default configuration failed to load"));
        }
        return ConfigHolder(config);
    }
    ConfigHolder config(FcConfigCreate());
    if (!config) {
        setDiagnostic(diagnostic, QStringLiteral("fontconfig failed to allocate a configuration"));
        return config;
    }
    const QByteArray path = QFileInfo(request.configurationFile).absoluteFilePath().toUtf8();
    if (FcConfigParseAndLoad(config.get(), reinterpret_cast<const FcChar8 *>(path.constData()), FcTrue) == FcFalse) {
        setDiagnostic(diagnostic, QStringLiteral("fontconfig failed to parse the injected configuration file"));
        return ConfigHolder(nullptr);
    }
    return config;
}

bool addInjectedDirectories(FcConfig *config, const FontDiscoveryRequest &request, QString *diagnostic)
{
    for (const QString &directory : request.fontDirectories) {
        const QFileInfo info(directory);
        if (!info.isDir()) {
            setDiagnostic(diagnostic,
                          QStringLiteral("injected font directory does not exist: %1").arg(directory.left(256)));
            return false;
        }
        const QByteArray path = info.absoluteFilePath().toUtf8();
        if (FcConfigAppFontAddDir(config, reinterpret_cast<const FcChar8 *>(path.constData())) == FcFalse) {
            setDiagnostic(diagnostic,
                          QStringLiteral("fontconfig refused the injected font directory: %1").arg(directory.left(256)));
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool readBoundedString(FcPattern *pattern, const char *object, int limitBytes, QString *output)
{
    FcChar8 *value = nullptr;
    if (FcPatternGetString(pattern, object, 0, &value) != FcResultMatch || value == nullptr) {
        return false;
    }
    const int length = static_cast<int>(std::char_traits<char>::length(reinterpret_cast<const char *>(value)));
    if (length > limitBytes) {
        // AGENT-GUARD: Over-long strings reject the whole pattern. Truncating
        // could alias two distinct families onto one identity.
        return false;
    }
    *output = QString::fromUtf8(reinterpret_cast<const char *>(value), length);
    return true;
}

[[nodiscard]] int readInteger(FcPattern *pattern, const char *object, int fallback)
{
    int value = 0;
    if (FcPatternGetInteger(pattern, object, 0, &value) != FcResultMatch) {
        return fallback;
    }
    return value;
}

[[nodiscard]] std::optional<FontFact> factFromPattern(FcPattern *pattern, int limitBytes)
{
    FontFact fact;
    if (!readBoundedString(pattern, FC_FAMILY, limitBytes, &fact.family)) {
        return std::nullopt;
    }
    QString style;
    if (readBoundedString(pattern, FC_STYLE, limitBytes, &style) && !style.trimmed().isEmpty()) {
        fact.style = style;
    }
    QString postscriptName;
    if (readBoundedString(pattern, FC_POSTSCRIPT_NAME, limitBytes, &postscriptName)) {
        fact.postscriptName = postscriptName;
    }
    fact.weight = weightToCssScale(readInteger(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR));
    fact.italic = readInteger(pattern, FC_SLANT, FC_SLANT_ROMAN) == FC_SLANT_ITALIC;
    fact.isMonospace = readInteger(pattern, FC_SPACING, FC_PROPORTIONAL) == FC_MONO;
    FcBool scalable = FcTrue;
    if (FcPatternGetBool(pattern, FC_SCALABLE, 0, &scalable) == FcResultMatch) {
        fact.isScalable = scalable == FcTrue;
    }
    if (!fact.isValid()) {
        return std::nullopt;
    }
    return fact;
}

} // namespace

FontDiscoveryProvider::FontDiscoveryProvider(FontDiscoveryRequest request)
    : m_request(std::move(request))
{
}

FontDiscoveryResult FontDiscoveryProvider::discover() const
{
    FontDiscoveryResult result;
    if (!m_request.isWellFormed()) {
        result.diagnostic = QStringLiteral("font discovery request is malformed");
        return result;
    }

    QString diagnostic;
    ConfigHolder config = buildConfig(m_request, &diagnostic);
    if (!config) {
        result.diagnostic = diagnostic;
        return result;
    }
    if (!addInjectedDirectories(config.get(), m_request, &diagnostic)) {
        result.diagnostic = diagnostic;
        return result;
    }
    if (FcConfigBuildFonts(config.get()) == FcFalse) {
        result.diagnostic = QStringLiteral("fontconfig failed to build the font set");
        return result;
    }

    FcPattern *pattern = FcPatternCreate();
    FcObjectSet *objects = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_WEIGHT, FC_SLANT, FC_SPACING,
                                            FC_SCALABLE, FC_POSTSCRIPT_NAME, FC_FILE, nullptr);
    FcFontSet *fontSet = (pattern != nullptr && objects != nullptr)
                             ? FcFontList(config.get(), pattern, objects)
                             : nullptr;
    if (pattern != nullptr) {
        FcPatternDestroy(pattern);
    }
    if (objects != nullptr) {
        FcObjectSetDestroy(objects);
    }
    if (fontSet == nullptr) {
        result.diagnostic = QStringLiteral("fontconfig could not enumerate the injected font set");
        return result;
    }

    QList<FontFact> facts;
    for (int i = 0; i < fontSet->nfont; ++i) {
        if (auto fact = factFromPattern(fontSet->fonts[i], m_request.limits.maximumStringBytes)) {
            facts.append(std::move(*fact));
        }
    }
    FcFontSetDestroy(fontSet);

    // AGENT-GUARD: Ordering is part of the contract. FcFontList order depends
    // on cache state; the published order is always this explicit sort.
    std::sort(facts.begin(), facts.end());
    if (facts.size() > m_request.limits.maximumFacts) {
        facts.resize(m_request.limits.maximumFacts);
        result.truncated = true;
    }
    result.available = true;
    result.facts = std::move(facts);
    return result;
}

} // namespace QindaQt::Services::FontDiscovery
