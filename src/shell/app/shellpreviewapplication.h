// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "previewoptions.h"

#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QObject>
#include <QQmlApplicationEngine>

#include <memory>

class QGuiApplication;

namespace QindaQt::Shell {

class ScreenshotCapture;

class ShellPreviewApplication final : public QObject {
    Q_OBJECT

public:
    explicit ShellPreviewApplication(QGuiApplication &application);
    ~ShellPreviewApplication() override;

    int run();

private:
    [[nodiscard]] QString resolveDataDirectory(const QString &explicitPath,
                                               const char *environmentName,
                                               const char *sourcePath,
                                               const QString &installedSuffix) const;
    [[nodiscard]] bool loadCatalogs(const PreviewOptions &options, QString *error);
    void printCatalog() const;
    [[nodiscard]] bool loadWindow(const PreviewOptions &options);
    void startCapture(const PreviewOptions &options);

    QGuiApplication &m_application;
    QindaQt::Profiles::ProfileCatalog m_profiles;
    QindaQt::Themes::ThemeCatalog m_themes;
    QQmlApplicationEngine m_engine;
    std::unique_ptr<ScreenshotCapture> m_capture;
};

} // namespace QindaQt::Shell
