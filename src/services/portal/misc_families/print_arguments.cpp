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
QStringList PrintArguments::destination(const QPrinter *printer, const QString &version)
{
    if (version == QLatin1String("lp")) {
        return QStringList(QStringLiteral("-d")) << printer->printerName();
    }

    if (version.startsWith(QLatin1String("lpr"))) {
        return QStringList(QStringLiteral("-P")) << printer->printerName();
    }

    return QStringList();
}

QStringList PrintArguments::copies(const QPrinter *printer, const QString &version)
{
    int cp = printer->copyCount();

    if (version == QLatin1String("lp")) {
        return QStringList(QStringLiteral("-n")) << QStringLiteral("%1").arg(cp);
    }

    if (version.startsWith(QLatin1String("lpr"))) {
        return QStringList() << QStringLiteral("-#%1").arg(cp);
    }

    return QStringList();
}

QStringList PrintArguments::jobname(const QPrinter *printer, const QString &version)
{
    if (!printer->docName().isEmpty()) {
        if (version == QLatin1String("lp")) {
            return QStringList(QStringLiteral("-t")) << printer->docName();
        }

        if (version.startsWith(QLatin1String("lpr"))) {
            const QString shortenedDocName = QString::fromUtf8(printer->docName().toUtf8().left(255));
            return QStringList(QStringLiteral("-J")) << shortenedDocName;
        }
    }

    return QStringList();
}

// What about Upper and MultiPurpose?  And others in PPD???
QString PrintArguments::mediaPaperSource(const QPrinter *printer)
{
    switch (printer->paperSource()) {
    case QPrinter::Auto:
        return QString();
    case QPrinter::Cassette:
        return QStringLiteral("Cassette");
    case QPrinter::Envelope:
        return QStringLiteral("Envelope");
    case QPrinter::EnvelopeManual:
        return QStringLiteral("EnvelopeManual");
    case QPrinter::FormSource:
        return QStringLiteral("FormSource");
    case QPrinter::LargeCapacity:
        return QStringLiteral("LargeCapacity");
    case QPrinter::LargeFormat:
        return QStringLiteral("LargeFormat");
    case QPrinter::Lower:
        return QStringLiteral("Lower");
    case QPrinter::MaxPageSource:
        return QStringLiteral("MaxPageSource");
    case QPrinter::Middle:
        return QStringLiteral("Middle");
    case QPrinter::Manual:
        return QStringLiteral("Manual");
    case QPrinter::OnlyOne:
        return QStringLiteral("OnlyOne");
    case QPrinter::Tractor:
        return QStringLiteral("Tractor");
    case QPrinter::SmallFormat:
        return QStringLiteral("SmallFormat");
    default:
        return QString();
    }
}

QStringList PrintArguments::optionOrientation(const QPrinter *printer, QPageLayout::Orientation documentOrientation)
{
    // portrait and landscape options rotate the document according to the document orientation
    // If we want to print a landscape document as one would expect it, we have to pass the
    // portrait option so that the document is not rotated additionally
    if (printer->pageLayout().orientation() == documentOrientation) {
        // the user wants the document printed as is
        return QStringList(QStringLiteral("-o")) << QStringLiteral("portrait");
    } else {
        // the user expects the document being rotated by 90 degrees
        return QStringList(QStringLiteral("-o")) << QStringLiteral("landscape");
    }
}

QStringList PrintArguments::optionDoubleSidedPrinting(const QPrinter *printer)
{
    switch (printer->duplex()) {
    case QPrinter::DuplexNone:
        return QStringList(QStringLiteral("-o")) << QStringLiteral("sides=one-sided");
    case QPrinter::DuplexAuto:
        if (printer->pageLayout().orientation() == QPageLayout::Landscape) {
            return QStringList(QStringLiteral("-o")) << QStringLiteral("sides=two-sided-short-edge");
        } else {
            return QStringList(QStringLiteral("-o")) << QStringLiteral("sides=two-sided-long-edge");
        }
    case QPrinter::DuplexLongSide:
        return QStringList(QStringLiteral("-o")) << QStringLiteral("sides=two-sided-long-edge");
    case QPrinter::DuplexShortSide:
        return QStringList(QStringLiteral("-o")) << QStringLiteral("sides=two-sided-short-edge");
    default:
        return QStringList(); // Use printer default
    }
}

QStringList PrintArguments::optionPageOrder(const QPrinter *printer)
{
    if (printer->pageOrder() == QPrinter::LastPageFirst) {
        return QStringList(QStringLiteral("-o")) << QStringLiteral("outputorder=reverse");
    }
    return QStringList(QStringLiteral("-o")) << QStringLiteral("outputorder=normal");
}

QStringList PrintArguments::optionCollateCopies(const QPrinter *printer)
{
    if (printer->collateCopies()) {
        return QStringList(QStringLiteral("-o")) << QStringLiteral("Collate=True");
    }
    return QStringList(QStringLiteral("-o")) << QStringLiteral("Collate=False");
}

QStringList PrintArguments::optionPageMargins(const QPrinter *printer)
{
    if (printer->printEngine()->property(QPrintEngine::PPK_PageMargins).isNull()) {
        return QStringList();
    } else {
        qreal l, t, r, b;
        QMarginsF m = printer->pageLayout().margins();
        l = m.left();
        t = m.top();
        r = m.right();
        b = m.bottom();
        return QStringList{
            QStringLiteral("-o"),
            QStringLiteral("page-left=%1").arg(l),
            QStringLiteral("-o"),
            QStringLiteral("page-top=%1").arg(t),
            QStringLiteral("-o"),
            QStringLiteral("page-right=%1").arg(r),
            QStringLiteral("-o"),
            QStringLiteral("page-bottom=%1").arg(b),
            QStringLiteral("-o"),
            QStringLiteral("fit-to-page"),
        };
    }
}

QStringList PrintArguments::optionCupsProperties(const QPrinter *printer)
{
    QStringList dialogOptions = printer->printEngine()->property(QPrintEngine::PrintEnginePropertyKey(0xfe00)).toStringList();
    QStringList cupsOptions;

    for (int i = 0; i + 1 < dialogOptions.count(); i = i + 2) {
        // Ignore some cups properties as the pdf we get is already formatted using these
        if (dialogOptions[i] == QLatin1String("number-up") || dialogOptions[i] == QLatin1String("number-up-layout")) {
            continue;
        }

        if (dialogOptions[i + 1].isEmpty()) {
            cupsOptions << QStringLiteral("-o") << dialogOptions[i];
        } else {
            cupsOptions << QStringLiteral("-o") << dialogOptions[i] + QLatin1Char('=') + dialogOptions[i + 1];
        }
    }

    return cupsOptions;
}

QStringList PrintArguments::optionMedia(const QPrinter *printer)
{
    if (!printPageKey(printer->pageLayout().pageSize().id()).isEmpty() && !mediaPaperSource(printer).isEmpty()) {
        return QStringList(QStringLiteral("-o"))
            << QStringLiteral("media=%1,%2").arg(printPageKey(printer->pageLayout().pageSize().id()), mediaPaperSource(printer));
    }

    if (!printPageKey(printer->pageLayout().pageSize().id()).isEmpty()) {
        return QStringList(QStringLiteral("-o")) << QStringLiteral("media=%1").arg(printPageKey(printer->pageLayout().pageSize().id()));
    }

    if (!mediaPaperSource(printer).isEmpty()) {
        return QStringList(QStringLiteral("-o")) << QStringLiteral("media=%1").arg(mediaPaperSource(printer));
    }

    return QStringList();
}

QStringList PrintArguments::pages(const QPrinter *printer, bool useCupsOptions, const QString &version)
{
    if (printer->printRange() == QPrinter::PageRange) {
        if (version == QLatin1String("lp")) {
            return QStringList(QStringLiteral("-P")) << QStringLiteral("%1-%2").arg(printer->fromPage()).arg(printer->toPage());
        }

        if (version.startsWith(QLatin1String("lpr")) && useCupsOptions) {
            return QStringList(QStringLiteral("-o")) << QStringLiteral("page-ranges=%1-%2").arg(printer->fromPage()).arg(printer->toPage());
        }
    }

    return QStringList(); // AllPages
}

QStringList PrintArguments::cupsOptions(const QPrinter *printer, QPageLayout::Orientation documentOrientation)
{
    Q_UNUSED(documentOrientation)
    QStringList optionList;

    // if (!optionMedia(printer).isEmpty()) {
    //     optionList << optionMedia(printer);
    // }

    // if (!optionOrientation(printer, documentOrientation).isEmpty()) {
    //     optionList << optionOrientation(printer, documentOrientation);
    // }

    if (!optionDoubleSidedPrinting(printer).isEmpty()) {
        optionList << optionDoubleSidedPrinting(printer);
    }

    if (!optionPageOrder(printer).isEmpty()) {
        optionList << optionPageOrder(printer);
    }

    if (!optionCollateCopies(printer).isEmpty()) {
        optionList << optionCollateCopies(printer);
    }

    // if (!optionPageMargins(printer).isEmpty()) {
    //     optionList << optionPageMargins(printer);
    // }

    optionList << optionCupsProperties(printer);

    return optionList;
}

QStringList PrintArguments::arguments(const QPrinter *printer, bool useCupsOptions, const QString &version, QPageLayout::Orientation documentOrientation)
{
    QStringList argList;

    if (!destination(printer, version).isEmpty()) {
        argList << destination(printer, version);
    }

    if (!copies(printer, version).isEmpty()) {
        argList << copies(printer, version);
    }

    if (!jobname(printer, version).isEmpty()) {
        argList << jobname(printer, version);
    }

    // if (!pages(printer, useCupsOptions, version).isEmpty()) {
    //     argList << pages(printer, useCupsOptions, version);
    // }

    if (useCupsOptions && !cupsOptions(printer, documentOrientation).isEmpty()) {
        argList << cupsOptions(printer, documentOrientation);
    }

    if (version == QLatin1String("lp")) {
        argList << QStringLiteral("--");
    }

    return argList;
}

}
