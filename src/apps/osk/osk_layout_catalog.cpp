// SPDX-License-Identifier: GPL-3.0-or-later
#include "osk_layout_catalog.h"

#include <QDir>
#include <QFileInfo>

namespace QindaQt::Apps::Osk {

namespace {
constexpr auto FallbackLayout = "us";
}

OskLayoutCatalog::OskLayoutCatalog(QString directory) : m_directory(std::move(directory)) {}

QString OskLayoutCatalog::defaultDirectory()
{
    return QStringLiteral(":/qt/qml/QindaQt/OskApp/layouts");
}

QString OskLayoutCatalog::normalize(const QString &xkbName)
{
    // `us(intl)` and `de,us` both mean the first named layout.
    QString name = xkbName.trimmed().toLower();
    const qsizetype comma = name.indexOf(QLatin1Char(','));
    if (comma >= 0) {
        name.truncate(comma);
    }
    const qsizetype paren = name.indexOf(QLatin1Char('('));
    if (paren >= 0) {
        name.truncate(paren);
    }
    return name.trimmed();
}

QStringList OskLayoutCatalog::available() const
{
    QStringList names;
    const QFileInfoList entries = QDir(m_directory).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
        names.append(entry.completeBaseName());
    }
    return names;
}

bool OskLayoutCatalog::has(const QString &xkbName) const
{
    const QString name = normalize(xkbName);
    return !name.isEmpty() && QFileInfo::exists(QDir(m_directory).filePath(name + QStringLiteral(".json")));
}

OskLayoutDocument OskLayoutCatalog::documentFor(const QString &xkbName, QString *error) const
{
    const QString name = has(xkbName) ? normalize(xkbName) : QString::fromLatin1(FallbackLayout);
    return OskLayoutDocument::load(QDir(m_directory).filePath(name + QStringLiteral(".json")), error);
}

} // namespace QindaQt::Apps::Osk
