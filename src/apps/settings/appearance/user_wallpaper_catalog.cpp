// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/user_wallpaper_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>

namespace QindaQt::Apps::SettingsAppearance {
namespace {
bool readableImage(const QString &path)
{
    const QFileInfo file(path);
    return file.isFile() && file.isReadable() && QImageReader(path).canRead();
}
}

UserWallpaperCatalog::UserWallpaperCatalog(QObject *parent, QString preferenceFile,
                                           QString defaultFolder)
    : QObject(parent), m_preferenceFile(std::move(preferenceFile))
{
    if (m_preferenceFile.isEmpty())
        m_preferenceFile = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/qindaqt/wallpaper-gallery.ini");
    if (defaultFolder.isEmpty()) {
        defaultFolder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (defaultFolder.isEmpty()) defaultFolder = QDir::homePath() + QStringLiteral("/Pictures");
        defaultFolder += QStringLiteral("/Wallpapers");
    }
    QSettings preferences(m_preferenceFile, QSettings::IniFormat);
    m_folder = preferences.value(QStringLiteral("folder"), defaultFolder).toString();
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &UserWallpaperCatalog::refresh);
    refresh();
}

void UserWallpaperCatalog::refresh()
{
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_wallpapers.clear();
    QDir directory(m_folder);
    if (directory.exists()) {
        m_watcher.addPath(m_folder);
        const auto files = directory.entryInfoList(QDir::Files | QDir::Readable,
                                                   QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &file : files) {
            if (!readableImage(file.absoluteFilePath())) continue;
            m_wallpapers.append(QVariantMap{
                {QStringLiteral("name"), file.completeBaseName()},
                {QStringLiteral("path"), file.absoluteFilePath()},
                {QStringLiteral("previewUrl"), QUrl::fromLocalFile(file.absoluteFilePath())},
                {QStringLiteral("value"), file.absoluteFilePath()}});
        }
    } else if (QFileInfo(directory.absolutePath()).dir().exists()) {
        // Observe creation of the default folder without creating it just by
        // opening Settings. Import creates it only when there is an image.
        m_watcher.addPath(QFileInfo(directory.absolutePath()).absolutePath());
    }
    emit changed();
}

bool UserWallpaperCatalog::setFolder(const QUrl &folder)
{
    const QFileInfo directory(folder.toLocalFile());
    if (!folder.isLocalFile() || !directory.isDir() || !directory.isReadable()) {
        m_error = tr("Choose a readable local folder.");
        emit changed();
        return false;
    }
    QSettings preferences(m_preferenceFile, QSettings::IniFormat);
    preferences.setValue(QStringLiteral("folder"), directory.absoluteFilePath());
    preferences.sync();
    if (preferences.status() != QSettings::NoError) {
        m_error = tr("Could not save the wallpaper folder.");
        emit changed();
        return false;
    }
    m_folder = directory.absoluteFilePath();
    m_error.clear();
    refresh();
    return true;
}

QString UserWallpaperCatalog::importImage(const QUrl &source)
{
    const QString sourcePath = source.toLocalFile();
    if (!source.isLocalFile() || !readableImage(sourcePath)) {
        m_error = tr("Choose a readable image file.");
        emit changed();
        return {};
    }
    QDir directory(m_folder);
    if (!directory.mkpath(QStringLiteral("."))) {
        m_error = tr("Could not create the wallpaper folder.");
        emit changed();
        return {};
    }
    const QFileInfo file(sourcePath);
    QString destination = directory.filePath(file.fileName());
    if (QFileInfo(destination).canonicalFilePath() != file.canonicalFilePath()) {
        // AGENT-GUARD: QFile::copy refuses existing destinations; never replace
        // another wallpaper (or a symlink target) merely because names match.
        int suffix = 2;
        while (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) {
            destination = directory.filePath(file.completeBaseName()
                + QStringLiteral(" (%1).").arg(suffix++) + file.suffix());
        }
        if (!QFile::copy(sourcePath, destination)) {
            m_error = tr("Could not copy the image into the wallpaper folder.");
            emit changed();
            return {};
        }
    }
    m_error.clear();
    refresh();
    return QFileInfo(destination).absoluteFilePath();
}

} // namespace QindaQt::Apps::SettingsAppearance
