// SPDX-License-Identifier: GPL-3.0-or-later
#include "capture_file_policy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QSaveFile>

namespace QindaQt::Screenshot {
namespace {

constexpr int MaxCollisions = 999;

QByteArray formatFor(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg"))
        return QByteArrayLiteral("jpg");
    if (suffix == QLatin1String("webp"))
        return QByteArrayLiteral("webp");
    return QByteArrayLiteral("png");
}

QString writerError(const QImageWriter &writer)
{
    return writer.errorString().isEmpty() ? QStringLiteral("the image could not be encoded")
                                          : writer.errorString();
}

} // namespace

QString uniquePath(const QString &directory, const QString &fileName, const PathExists &exists)
{
    const QDir dir(directory);
    const QString first = dir.filePath(fileName);
    if (!exists(first))
        return first;
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    for (int index = 2; index <= MaxCollisions; ++index) {
        const QString candidate = dir.filePath(QStringLiteral("%1-%2%3").arg(base).arg(index).arg(suffix));
        if (!exists(candidate))
            return candidate;
    }
    return {};
}

SaveResult saveWithoutOverwriting(const QImage &image, const QString &directory,
                                  const QString &fileName)
{
    if (image.isNull())
        return {{}, QStringLiteral("there is no screenshot to save")};
    if (!QDir().mkpath(directory))
        return {{}, QStringLiteral("could not create %1").arg(directory)};
    const PathExists exists = [](const QString &path) { return QFileInfo::exists(path); };
    // AGENT-GUARD: NewOnly is O_EXCL. uniquePath() only proposes a name; the
    // exclusive open is what actually guarantees another file is never
    // replaced, so a name taken between the two steps just moves on.
    for (int attempt = 0; attempt < 8; ++attempt) {
        const QString path = uniquePath(directory, fileName, exists);
        if (path.isEmpty())
            return {{}, QStringLiteral("too many screenshots share the name %1").arg(fileName)};
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            if (QFileInfo::exists(path))
                continue;
            return {{}, file.errorString()};
        }
        QImageWriter writer(&file, formatFor(path));
        if (!writer.write(image)) {
            const QString error = writerError(writer);
            file.close();
            file.remove();
            return {{}, error};
        }
        file.close();
        return {path, {}};
    }
    return {{}, QStringLiteral("could not find a free file name in %1").arg(directory)};
}

SaveResult saveToConfirmedPath(const QImage &image, const QString &path)
{
    if (image.isNull())
        return {{}, QStringLiteral("there is no screenshot to save")};
    if (path.isEmpty())
        return {{}, QStringLiteral("no file name was chosen")};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {{}, file.errorString()};
    QImageWriter writer(&file, formatFor(path));
    if (!writer.write(image)) {
        file.cancelWriting();
        return {{}, writerError(writer)};
    }
    if (!file.commit())
        return {{}, file.errorString()};
    return {path, {}};
}

} // namespace QindaQt::Screenshot
