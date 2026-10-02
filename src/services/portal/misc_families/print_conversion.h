/*
 * SPDX-FileCopyrightText: 2016-2017 Jan Grulich <jgrulich@redhat.com>
 * SPDX-FileCopyrightText: 2007, 2010 John Layt <john@layt.net>
 * SPDX-FileCopyrightText: 2007 Alex Merry <huntedhacker@tiscali.co.uk>
 * SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 */

#pragma once
#include <QPrinter>
#include <QJsonObject>
#include <QVariantMap>
namespace QindaQt::Services::Portal {
void loadPrintSettings(QPrinter *, const QVariantMap &, const QVariantMap &);
QJsonObject savePrintSettings(const QPrinter *);
void loadPrintConfiguration(QPrinter *, const QJsonObject &);
QString printPageKey(QPageSize::PageSizeId);
QPageSize::PageSizeId printPageId(const QString &);
// Conversion/arguments are the upstream Qt/CUPS contract, isolated from actor
// admission and token ownership. Input has already passed bounded wire policy.
class PrintArguments {
public:
    static QStringList arguments(const QPrinter *, bool, const QString &, QPageLayout::Orientation);
private:
    static QStringList destination(const QPrinter *, const QString &);
    static QStringList copies(const QPrinter *, const QString &);
    static QStringList jobname(const QPrinter *, const QString &);
    static QStringList cupsOptions(const QPrinter *, QPageLayout::Orientation);
    static QStringList pages(const QPrinter *, bool, const QString &);
    static QStringList optionMedia(const QPrinter *);
    static QString mediaPaperSource(const QPrinter *);
    static QStringList optionOrientation(const QPrinter *, QPageLayout::Orientation);
    static QStringList optionDoubleSidedPrinting(const QPrinter *);
    static QStringList optionPageOrder(const QPrinter *);
    static QStringList optionCollateCopies(const QPrinter *);
    static QStringList optionPageMargins(const QPrinter *);
    static QStringList optionCupsProperties(const QPrinter *);
};
}
