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
void loadPrintSettings(QPrinter *printer, const QVariantMap &settings, const QVariantMap &page_setup) {
    // First we have to load pre-configured options

    // Process settings (used by printer)
    QCUPSSupport::PagesPerSheet pagesPerSheet = QCUPSSupport::OnePagePerSheet;
    QCUPSSupport::PagesPerSheetLayout pagesPerSheetLayout = QCUPSSupport::LeftToRightTopToBottom;
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (it.key() == QStringLiteral("orientation")) {
            const QString orientation = it.value().toString();
            if (orientation == QLatin1String("landscape") || orientation == QLatin1String("reverse_landscape")) {
                printer->setPageOrientation(QPageLayout::Landscape);
            } else if (orientation == QLatin1String("portrait") || orientation == QLatin1String("reverse_portrait")) {
                printer->setPageOrientation(QPageLayout::Portrait);
            }
        } else if (it.key() == QStringLiteral("n-copies")) {
            printer->setCopyCount(it.value().toString().toInt());
        } else if (it.key() == QStringLiteral("resolution")) {
            printer->setResolution(it.value().toString().toInt());
        } else if (it.key() == QStringLiteral("use-color")) {
            printer->setColorMode(it.value().toString() == QLatin1String("yes") ? QPrinter::Color : QPrinter::GrayScale);
        } else if (it.key() == QStringLiteral("duplex")) {
            const QString duplex = it.value().toString();
            if (duplex == QLatin1String("simplex")) {
                printer->setDuplex(QPrinter::DuplexNone);
            } else if (duplex == QLatin1String("horizontal")) {
                printer->setDuplex(QPrinter::DuplexShortSide);
            } else if (duplex == QLatin1String("vertical")) {
                printer->setDuplex(QPrinter::DuplexLongSide);
            }
        } else if (it.key() == QStringLiteral("collate")) {
            printer->setCollateCopies(it.value().toString() == QLatin1String("yes"));
        } else if (it.key() == QStringLiteral("reverse")) {
            printer->setPageOrder(it.value().toString() == QLatin1String("yes") ? QPrinter::LastPageFirst : QPrinter::FirstPageFirst);
        } else if (it.key() == QStringLiteral("print-pages")) {
            const QString printPages = it.value().toString();
            if (printPages == QLatin1String("all")) {
                printer->setPrintRange(QPrinter::AllPages);
            } else if (printPages == QLatin1String("selection")) {
                printer->setPrintRange(QPrinter::Selection);
            } else if (printPages == QLatin1String("current")) {
                printer->setPrintRange(QPrinter::CurrentPage);
            } else if (printPages == QLatin1String("ranges")) {
                printer->setPrintRange(QPrinter::PageRange);
            }
        } else if (it.key() == QStringLiteral("page-ranges")) {
            const QString range = it.value().toString();
            // Gnome supports format like 1-5,7,9,11-15, however Qt support only e.g.1-15
            // so get the first and the last value
            const QStringList ranges = range.split(QLatin1Char(','));
            if (ranges.count()) {
                QStringList firstRangeValues = ranges.first().split(QLatin1Char('-'));
                QStringList lastRangeValues = ranges.last().split(QLatin1Char('-'));
                printer->setFromTo(firstRangeValues.first().toInt(), lastRangeValues.last().toInt());
            }
        } else if (it.key() == QStringLiteral("page-set")) {
            // WARNING Qt internal private API, anyway the print dialog doesn't seem to
            // read these properties, but I'll leave it here in case this changes in future
            const QString pageSet = it.value().toString();
            if (pageSet == QLatin1String("all")) {
                QCUPSSupport::setPageSet(printer, QCUPSSupport::AllPages);
            } else if (pageSet == QLatin1String("even")) {
                QCUPSSupport::setPageSet(printer, QCUPSSupport::EvenPages);
            } else if (pageSet == QLatin1String("odd")) {
                QCUPSSupport::setPageSet(printer, QCUPSSupport::OddPages);
            }
        } else if (it.key() == QStringLiteral("number-up")) {
            // WARNING Qt internal private API, anyway the print dialog doesn't seem to
            // read these properties, but I'll leave it here in case this changes in future
            const QString numberUp = it.value().toString();
            if (numberUp == QLatin1String("1")) {
                pagesPerSheet = QCUPSSupport::OnePagePerSheet;
            } else if (numberUp == QLatin1String("2")) {
                pagesPerSheet = QCUPSSupport::TwoPagesPerSheet;
            } else if (numberUp == QLatin1String("4")) {
                pagesPerSheet = QCUPSSupport::FourPagesPerSheet;
            } else if (numberUp == QLatin1String("6")) {
                pagesPerSheet = QCUPSSupport::SixPagesPerSheet;
            } else if (numberUp == QLatin1String("9")) {
                pagesPerSheet = QCUPSSupport::NinePagesPerSheet;
            } else if (numberUp == QLatin1String("16")) {
                pagesPerSheet = QCUPSSupport::SixteenPagesPerSheet;
            }
        } else if (it.key() == QStringLiteral("number-up-layout")) {
            // WARNING Qt internal private API, anyway the print dialog doesn't seem to
            // read these properties, but I'll leave it here in case this changes in future
            const QString layout = it.value().toString();
            if (layout == QLatin1String("lrtb")) {
                pagesPerSheetLayout = QCUPSSupport::LeftToRightTopToBottom;
            } else if (layout == QLatin1String("lrbt")) {
                pagesPerSheetLayout = QCUPSSupport::LeftToRightBottomToTop;
            } else if (layout == QLatin1String("rltb")) {
                pagesPerSheetLayout = QCUPSSupport::RightToLeftTopToBottom;
            } else if (layout == QLatin1String("rlbt")) {
                pagesPerSheetLayout = QCUPSSupport::RightToLeftBottomToTop;
            } else if (layout == QLatin1String("tblr")) {
                pagesPerSheetLayout = QCUPSSupport::TopToBottomLeftToRight;
            } else if (layout == QLatin1String("tbrl")) {
                pagesPerSheetLayout = QCUPSSupport::TopToBottomRightToLeft;
            } else if (layout == QLatin1String("btlr")) {
                pagesPerSheetLayout = QCUPSSupport::BottomToTopLeftToRight;
            } else if (layout == QLatin1String("btrl")) {
                pagesPerSheetLayout = QCUPSSupport::BottomToTopRightToLeft;
            }
        } else if (it.key() == QStringLiteral("output-file-format")) {
            // TODO only PDF supported by Qt so when printing to file we can use PDF only
            // btw. this should be already set automatically because we set output file name
            printer->setOutputFormat(QPrinter::PdfFormat);
        } else if (it.key() == QStringLiteral("output-uri")) {
            const QUrl uri = QUrl(it.value().toString());
            // AGENT-GUARD: QPrinter requires a local path, never file:// text.
            // Preserve a complete URI path even when output-basename is also
            // supplied; only directory URIs append the basename.
            QString path = uri.toLocalFile();
            if (uri.isLocalFile() && path.endsWith(QLatin1Char('/')) && settings.contains(QStringLiteral("output-basename")))
                path += settings.value(QStringLiteral("output-basename")).toString();
            printer->setOutputFileName(path);
        } else {
        }
    }

    QCUPSSupport::setPagesPerSheetLayout(printer, pagesPerSheet, pagesPerSheetLayout);
    // AGENT-GUARD: Qt 6.11.1's private enum/string table swaps some layouts.
    // Preserve the standard CUPS spelling requested by the frontend rather
    // than returning rlbt for rltb. The dialog still owns subsequent edits.
    const QString requestedLayout = settings.value(QStringLiteral("number-up-layout")).toString();
    if (QStringList{"lrtb", "lrbt", "rltb", "rlbt", "tblr", "tbrl", "btlr", "btrl"}.contains(requestedLayout))
        QCUPSSupport::setCupsOption(printer, QStringLiteral("number-up-layout"), requestedLayout);

    // Process page setup
    QSizeF paperSize;
    QString ppdName;
    QString name;
    QMarginsF pageMargins = printer->pageLayout().margins(QPageLayout::Millimeter);
    for (auto it = page_setup.constBegin(); it != page_setup.constEnd(); ++it) {
        if (it.key() == QStringLiteral("PPDName")) {
            ppdName = it.value().toString();
        } else if (it.key() == QStringLiteral("Name")) {
            name = it.value().toString();
        }

        if (it.key() == QStringLiteral("Width")) {
            paperSize.setWidth(it.value().toReal());
        } else if (it.key() == QStringLiteral("Height")) {
            paperSize.setHeight(it.value().toReal());
        } else if (it.key() == QStringLiteral("MarginTop")) {
            pageMargins.setTop(it.value().toReal());
        } else if (it.key() == QStringLiteral("MarginBottom")) {
            pageMargins.setBottom(it.value().toReal());
        } else if (it.key() == QStringLiteral("MarginLeft")) {
            pageMargins.setLeft(it.value().toReal());
        } else if (it.key() == QStringLiteral("MarginRight")) {
            pageMargins.setRight(it.value().toReal());
        } else if (it.key() == QStringLiteral("Orientation")) {
            const QString orientation = it.value().toString();
            if (orientation == QLatin1String("landscape") || orientation == QLatin1String("reverse_landscape")) {
                printer->setPageOrientation(QPageLayout::Landscape);
            } else if (orientation == QLatin1String("portrait") || orientation == QLatin1String("reverse_portrait")) {
                printer->setPageOrientation(QPageLayout::Portrait);
            }
        } else {
        }
    }

    if (!ppdName.isEmpty()) {
        printer->setPageSize(QPageSize(printPageId(ppdName)));
    } else if (!name.isEmpty()) {
        printer->setPageSize(QPageSize(paperSize, QPageSize::Millimeter, name));
    }
    printer->setPageMargins(pageMargins, QPageLayout::Millimeter);

}
void loadPrintConfiguration(QPrinter *printer, const QJsonObject &configuration) {
    printer->setPrinterName(configuration.value("printer").toString());
    loadPrintSettings(printer, configuration.value("settings").toObject().toVariantMap(), configuration.value("page-setup").toObject().toVariantMap());
    printer->setOutputFileName(configuration.value("output").toString());
    QStringList cups; for (const auto &value : configuration.value("cups").toArray()) cups.append(value.toString());
    printer->printEngine()->setProperty(QPrintEngine::PrintEnginePropertyKey(0xfe00), cups);
}
}
