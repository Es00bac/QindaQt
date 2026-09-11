// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/evdev_layout_catalog.h>

#include <QFile>
#include <QXmlStreamReader>

namespace QindaQt::Apps::SettingsInput {
namespace EvdevLayoutCatalog {
namespace {

constexpr qint64 MaximumFileBytes = 4 * 1024 * 1024;
constexpr int MaximumLayouts = 1024;
constexpr int MaximumVariantsPerLayout = 256;
constexpr long long MaximumElements = 200000;
constexpr qsizetype MaximumIdentifierLength = 32;
constexpr qsizetype MaximumDescriptionLength = 128;

// AGENT-GUARD: This parser runs on an installed system file today, but the
// path is injected and test-visible, so every bound is load-bearing: no DTD
// (entity expansion bombs), bounded element churn, bounded identifiers and
// descriptions (UI string injection). Any violation aborts the whole
// catalog instead of yielding partial data.

bool boundedText(QXmlStreamReader &xml, qsizetype maximum, QString *into) {
    const QString text = xml.readElementText();
    if (text.size() > maximum) {
        return false;
    }
    *into = text;
    return true;
}

bool readConfigItem(QXmlStreamReader &xml, QString *code,
                    QString *description) {
    while (xml.readNextStartElement()) {
        const QString item = xml.name().toString();
        if (item == QLatin1String("name")) {
            if (!boundedText(xml, MaximumIdentifierLength, code)) {
                return false;
            }
        } else if (item == QLatin1String("description")) {
            if (!boundedText(xml, MaximumDescriptionLength, description)) {
                return false;
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    return true;
}

} // namespace

QList<EvdevLayoutOption> parse(const QString &xmlPath, QString *error) {
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not open the layout catalog %1")
                         .arg(xmlPath);
        }
        return {};
    }
    if (file.size() > MaximumFileBytes) {
        if (error != nullptr) {
            *error = QStringLiteral("The layout catalog %1 is too large")
                         .arg(xmlPath);
        }
        return {};
    }
    QXmlStreamReader xml(&file);
    QList<EvdevLayoutOption> layouts;
    long long elements = 0;
    bool bounded = true;
    // bump() keeps one global element budget across all nesting levels so a
    // deeply or repeatedly nested document cannot churn unbounded CPU.
    const auto bump = [&elements, &bounded]() {
        ++elements;
        if (elements > MaximumElements) {
            bounded = false;
        }
        return bounded;
    };
    while (bounded && !xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::StartDocument) {
            if (token == QXmlStreamReader::DTD) {
                bounded = false; // DTDs enable expansion attacks; reject
                break;
            }
            continue;
        }
        if (token != QXmlStreamReader::StartElement || !bump()) {
            continue;
        }
        if (xml.name() != QLatin1String("layout")) {
            continue;
        }
        if (layouts.size() >= qsizetype(MaximumLayouts)) {
            bounded = false;
            break;
        }
        EvdevLayoutOption option;
        bool rowValid = true;
        while (bounded && rowValid && xml.readNextStartElement() && bump()) {
            const QString child = xml.name().toString();
            if (child == QLatin1String("configItem")) {
                rowValid = readConfigItem(xml, &option.code,
                                          &option.description);
            } else if (child == QLatin1String("variantList")) {
                while (bounded && rowValid && xml.readNextStartElement() &&
                       bump()) {
                    if (xml.name() != QLatin1String("variant")) {
                        xml.skipCurrentElement();
                        continue;
                    }
                    if (option.variants.size() >= MaximumVariantsPerLayout) {
                        rowValid = false;
                        break;
                    }
                    EvdevVariantOption variant;
                    while (bounded && rowValid &&
                           xml.readNextStartElement() && bump()) {
                        if (xml.name() != QLatin1String("configItem")) {
                            xml.skipCurrentElement();
                            continue;
                        }
                        rowValid = readConfigItem(xml, &variant.code,
                                                  &variant.description);
                    }
                    if (!variant.code.isEmpty()) {
                        option.variants.append(variant);
                    }
                }
            } else {
                xml.skipCurrentElement();
            }
        }
        if (!bounded || !rowValid) {
            break;
        }
        if (!option.code.isEmpty()) {
            layouts.append(option);
        }
    }
    if (xml.hasError() || !bounded) {
        if (error != nullptr) {
            *error = QStringLiteral("The layout catalog %1 is not a "
                                    "well-formed bounded catalog")
                         .arg(xmlPath);
        }
        return {};
    }
    return layouts;
}

} // namespace EvdevLayoutCatalog

} // namespace QindaQt::Apps::SettingsInput
