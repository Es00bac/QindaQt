/*
 * SPDX-FileCopyrightText: 2016-2017 Jan Grulich <jgrulich@redhat.com>
 * SPDX-FileCopyrightText: 2007, 2010 John Layt <john@layt.net>
 * SPDX-FileCopyrightText: 2007 Alex Merry <huntedhacker@tiscali.co.uk>
 * SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 */

#include "print_conversion.h"
#include <QPrintEngine>
#include <QUrl>
#include <QJsonArray>
#include <QtPrintSupport/private/qcups_p.h>
namespace QindaQt::Services::Portal {
QJsonObject savePrintSettings(const QPrinter *printer) {
            QVariantMap resultingSettings;
            QVariantMap resultingPageSetup;

            // Process back printer settings
            resultingSettings.insert(QStringLiteral("n-copies"), QString::number(printer->copyCount()));
            resultingSettings.insert(QStringLiteral("resolution"), QString::number(printer->resolution()));
            resultingSettings.insert(QStringLiteral("use-color"), printer->colorMode() == QPrinter::Color ? QLatin1String("yes") : QLatin1String("no"));
            if (printer->duplex() == QPrinter::DuplexNone) {
                resultingSettings.insert(QStringLiteral("duplex"), QLatin1String("simplex"));
            } else if (printer->duplex() == QPrinter::DuplexShortSide) {
                resultingSettings.insert(QStringLiteral("duplex"), QLatin1String("horizontal"));
            } else if (printer->duplex() == QPrinter::DuplexLongSide) {
                resultingSettings.insert(QStringLiteral("duplex"), QLatin1String("vertical"));
            }
            resultingSettings.insert(QStringLiteral("collate"), printer->collateCopies() ? QLatin1String("yes") : QLatin1String("no"));
            resultingSettings.insert(QStringLiteral("reverse"), printer->pageOrder() == QPrinter::LastPageFirst ? QLatin1String("yes") : QLatin1String("no"));
            if (printer->printRange() == QPrinter::AllPages) {
                resultingSettings.insert(QStringLiteral("print-pages"), QLatin1String("all"));
            } else if (printer->printRange() == QPrinter::Selection) {
                resultingSettings.insert(QStringLiteral("print-pages"), QLatin1String("selection"));
            } else if (printer->printRange() == QPrinter::CurrentPage) {
                resultingSettings.insert(QStringLiteral("print-pages"), QLatin1String("current"));
            } else if (printer->printRange() == QPrinter::PageRange) {
                resultingSettings.insert(QStringLiteral("print-pages"), QLatin1String("ranges"));
                const int fromPageToIndex = printer->fromPage() ? printer->fromPage() - 1 : printer->fromPage();
                const int toPageToIndex = printer->toPage() ? printer->toPage() - 1 : printer->toPage();
                resultingSettings.insert(QStringLiteral("page-ranges"), QStringLiteral("%1-%2").arg(fromPageToIndex).arg(toPageToIndex));
            }
            // Set cups specific properties
            const QStringList cupsOptions = printer->printEngine()->property(PPK_CupsOptions).toStringList();
            if ((cupsOptions.indexOf(QLatin1String("page-set")) >= 0 && cupsOptions.indexOf(QLatin1String("page-set")) + 1 < cupsOptions.size())) {
                resultingSettings.insert(QStringLiteral("page-set"), cupsOptions.at(cupsOptions.indexOf(QStringLiteral("page-set")) + 1));
            }
            if ((cupsOptions.indexOf(QLatin1String("number-up")) >= 0 && cupsOptions.indexOf(QLatin1String("number-up")) + 1 < cupsOptions.size())) {
                resultingSettings.insert(QStringLiteral("number-up"), cupsOptions.at(cupsOptions.indexOf(QStringLiteral("number-up")) + 1));
            }
            if ((cupsOptions.indexOf(QLatin1String("number-up-layout")) >= 0 && cupsOptions.indexOf(QLatin1String("number-up-layout")) + 1 < cupsOptions.size())) {
                resultingSettings.insert(QStringLiteral("number-up-layout"), cupsOptions.at(cupsOptions.indexOf(QStringLiteral("number-up-layout")) + 1));
            }

            if (printer->outputFormat() == QPrinter::PdfFormat) {
                resultingSettings.insert(QStringLiteral("output-file-format"), QLatin1String("pdf"));
            }

            if (!printer->outputFileName().isEmpty()) {
                resultingSettings.insert(QStringLiteral("output-uri"), QUrl::fromLocalFile(printer->outputFileName()).toDisplayString());
            }

            // Process back page setup
            if (printer->pageLayout().pageSize().id() == QPageSize::Custom) {
                resultingPageSetup.insert(QStringLiteral("Name"), printer->pageLayout().pageSize().name());
                resultingPageSetup.insert(QStringLiteral("DisplayName"), printer->pageLayout().pageSize().name());
            } else {
                resultingPageSetup.insert(QStringLiteral("PPDName"), printPageKey(printer->pageLayout().pageSize().id()));
                resultingPageSetup.insert(QStringLiteral("DisplayName"), printPageKey(printer->pageLayout().pageSize().id()));
            }
            resultingPageSetup.insert(QStringLiteral("Width"), printer->pageLayout().pageSize().size(QPageSize::Millimeter).width());
            resultingPageSetup.insert(QStringLiteral("Height"), printer->pageLayout().pageSize().size(QPageSize::Millimeter).height());
            resultingPageSetup.insert(QStringLiteral("MarginTop"), printer->pageLayout().margins(QPageLayout::Millimeter).top());
            resultingPageSetup.insert(QStringLiteral("MarginBottom"), printer->pageLayout().margins(QPageLayout::Millimeter).bottom());
            resultingPageSetup.insert(QStringLiteral("MarginLeft"), printer->pageLayout().margins(QPageLayout::Millimeter).left());
            resultingPageSetup.insert(QStringLiteral("MarginRight"), printer->pageLayout().margins(QPageLayout::Millimeter).right());
            resultingPageSetup.insert(QStringLiteral("Orientation"),
                                      printer->pageLayout().orientation() == QPageLayout::Landscape ? QLatin1String("landscape") : QLatin1String("portrait"));


    resultingSettings.insert("orientation", printer->pageLayout().orientation() == QPageLayout::Landscape ? "landscape" : "portrait");

    QJsonArray cups;
    for (const auto &value : printer->printEngine()->property(QPrintEngine::PrintEnginePropertyKey(0xfe00)).toStringList()) cups.append(value);
    return QJsonObject{{"settings", QJsonObject::fromVariantMap(resultingSettings)}, {"page-setup", QJsonObject::fromVariantMap(resultingPageSetup)},
        {"printer", printer->printerName()}, {"output", printer->outputFileName()}, {"cups", cups}};
}
}
