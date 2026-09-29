// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/screenshot_preferences/file_name_pattern.h>

#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Services::ScreenshotPreferences {
namespace {

QString expanded(const QString &pattern, const QDateTime &time, const QString &modeId)
{
    QString name = pattern;
    name.replace(QStringLiteral("{date}"), time.toString(QStringLiteral("yyyy-MM-dd")));
    name.replace(QStringLiteral("{time}"), time.toString(QStringLiteral("HH-mm-ss")));
    name.replace(QStringLiteral("{mode}"), modeId.isEmpty() ? QStringLiteral("capture") : modeId);
    return name.trimmed();
}

bool safeName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..")
           && !name.contains(QLatin1Char('/')) && !name.contains(QChar(0));
}

} // namespace

bool isValidFileNamePattern(const QString &pattern)
{
    if (pattern.trimmed().isEmpty() || pattern.size() > kMaxFileNamePatternLength)
        return false;
    // Expanding with a fixed sample catches a pattern made only of tokens
    // that could expand to something unsafe.
    return safeName(pattern) && safeName(expanded(pattern, QDateTime(QDate(2000, 1, 1), QTime(0, 0)),
                                                  QStringLiteral("region")));
}

QString expandFileNamePattern(const QString &pattern, const QDateTime &time, const QString &modeId)
{
    const QString usable = isValidFileNamePattern(pattern) ? pattern
                                                           : QString::fromLatin1(kDefaultFileNamePattern);
    QString name = expanded(usable, time, modeId);
    if (!safeName(name))
        name = expanded(QString::fromLatin1(kDefaultFileNamePattern), time, modeId);
    return name + QStringLiteral(".png");
}

QString defaultScreenshotFolder()
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QDir::home().filePath(QStringLiteral("Pictures"));
    return QDir(pictures).filePath(QStringLiteral("Screenshots"));
}

QString resolveScreenshotFolder(const QString &configured)
{
    QString folder = configured.trimmed();
    if (folder == QLatin1String("~"))
        folder = QDir::homePath();
    else if (folder.startsWith(QLatin1String("~/")))
        folder = QDir::home().filePath(folder.mid(2));
    if (folder.isEmpty() || !QDir::isAbsolutePath(folder) || folder.contains(QChar(0)))
        return defaultScreenshotFolder();
    return QDir::cleanPath(folder);
}

} // namespace QindaQt::Services::ScreenshotPreferences
