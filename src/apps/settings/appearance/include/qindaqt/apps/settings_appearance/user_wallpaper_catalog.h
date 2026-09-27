// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QUrl>
#include <QVariantList>

namespace QindaQt::Apps::SettingsAppearance {

// GUI-thread, parent-owned local image library. Folder preference is local UI
// state; imported image selection still goes through Appearance's Settings1 draft.
class UserWallpaperCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit UserWallpaperCatalog(QObject *parent = nullptr,
                                  QString preferenceFile = {}, QString defaultFolder = {});
    [[nodiscard]] QString folder() const { return m_folder; }
    [[nodiscard]] QVariantList wallpapers() const { return m_wallpapers; }
    [[nodiscard]] QString error() const { return m_error; }
    // QML passes URL values unchanged; only C++ converts them to local paths.
    // Copies never overwrite or move the source. Failure returns empty + error.
    Q_INVOKABLE QString importImage(const QUrl &source);
    Q_INVOKABLE bool setFolder(const QUrl &folder);
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    QString m_preferenceFile;
    QString m_folder;
    QString m_error;
    QVariantList m_wallpapers;
    QFileSystemWatcher m_watcher;
};

} // namespace QindaQt::Apps::SettingsAppearance
