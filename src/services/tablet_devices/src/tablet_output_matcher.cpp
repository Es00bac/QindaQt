// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_output_matcher.h>

#include <QRegularExpression>
#include <QSet>

namespace QindaQt::Services::TabletDevices {
namespace {

// AGENT-NOTE: KWin publishes the EDID vendor as the decoded name when the
// PNP id is known ("Lenovo Group Limited" on the laptop's own panel,
// verified 2026-09-17) and as the raw three-letter code otherwise. Both
// forms must match, so the table holds normalized prefixes.
const QStringList &displayTabletVendorPrefixes() {
    static const QStringList prefixes{
        QStringLiteral("WAC"),   // Wacom: "WAC" and "WACOMTECHNOLOGY…"
        QStringLiteral("HUION"), //
        QStringLiteral("XPPEN"), // "XP-PEN" normalizes to XPPEN
        QStringLiteral("UGEE"),  // XP-Pen's EDID identity on several models
    };
    return prefixes;
}

// Words that appear on every tablet and every monitor and therefore
// distinguish nothing. Matching on one of these would map an opaque tablet
// to an unrelated screen.
const QSet<QString> &genericWords() {
    static const QSet<QString> words{
        QStringLiteral("PEN"),     QStringLiteral("DISPLAY"),
        QStringLiteral("TABLET"),  QStringLiteral("PAD"),
        QStringLiteral("TOUCH"),   QStringLiteral("SCREEN"),
        QStringLiteral("MONITOR"), QStringLiteral("STYLUS"),
        QStringLiteral("USB"),     QStringLiteral("DEVICE"),
        QStringLiteral("INC"),     QStringLiteral("LTD"),
        QStringLiteral("CORP"),    QStringLiteral("CO"),
        QStringLiteral("THE"),     QStringLiteral("AND"),
        QStringLiteral("ONE"),
    };
    return words;
}

QString normalized(const QString &text) {
    static const QRegularExpression nonAlphanumeric(
        QStringLiteral("[^A-Za-z0-9]+"));
    return text.toUpper().remove(nonAlphanumeric);
}

QSet<QString> distinctiveWords(const QString &text) {
    static const QRegularExpression separators(QStringLiteral("[^A-Za-z0-9]+"));
    QSet<QString> words;
    const QStringList parts =
        text.toUpper().split(separators, Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.size() < 3 || genericWords().contains(part)) {
            continue;
        }
        words.insert(part);
    }
    return words;
}

} // namespace

bool isDisplayTabletVendor(const QString &manufacturer) {
    const QString candidate = normalized(manufacturer);
    if (candidate.isEmpty()) {
        return false;
    }
    const QStringList &prefixes = displayTabletVendorPrefixes();
    for (const QString &prefix : prefixes) {
        if (candidate.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

TabletOutputMatch
matchTabletOutput(const TabletDeviceSnapshot &device,
                  const QList<TabletOutputCandidate> &outputs) {
    // A pad alone carries no pointer to map; only a tool decides an output.
    if (!device.tabletTool) {
        return {};
    }
    QStringList vendorMatches;
    for (const TabletOutputCandidate &output : outputs) {
        if (!output.enabled || output.connectorName.isEmpty()) {
            continue;
        }
        // AGENT-GUARD: An internal panel is never the pen display. Mapping a
        // pen to the laptop's own screen is exactly the defect this feature
        // exists to remove.
        if (output.internal) {
            continue;
        }
        if (isDisplayTabletVendor(output.manufacturer)) {
            vendorMatches.append(output.connectorName);
        }
    }
    if (vendorMatches.size() == 1) {
        return {TabletMatchReason::DisplayTabletVendor, vendorMatches.first()};
    }
    if (vendorMatches.size() > 1) {
        return {TabletMatchReason::Ambiguous, {}};
    }

    const QSet<QString> deviceWords = distinctiveWords(device.name);
    if (deviceWords.isEmpty()) {
        return {};
    }
    QStringList nameMatches;
    for (const TabletOutputCandidate &output : outputs) {
        if (!output.enabled || output.connectorName.isEmpty() ||
            output.internal) {
            continue;
        }
        QSet<QString> outputWords = distinctiveWords(output.model);
        outputWords.unite(distinctiveWords(output.label));
        if (outputWords.intersects(deviceWords)) {
            nameMatches.append(output.connectorName);
        }
    }
    if (nameMatches.size() == 1) {
        return {TabletMatchReason::ProductName, nameMatches.first()};
    }
    if (nameMatches.size() > 1) {
        return {TabletMatchReason::Ambiguous, {}};
    }
    return {};
}

} // namespace QindaQt::Services::TabletDevices
