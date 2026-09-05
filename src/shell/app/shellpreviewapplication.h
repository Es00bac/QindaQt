// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "previewoptions.h"
#include "../common/shelliconconfiguration.h"

#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QObject>
#include <QQmlApplicationEngine>

#include <memory>

class QGuiApplication;

namespace QindaQt::Shell {

class ScreenshotCapture;
class ShellTokenPublisher;

class ShellPreviewApplication final : public QObject {
    Q_OBJECT

public:
    explicit ShellPreviewApplication(QGuiApplication &application);
    ~ShellPreviewApplication() override;

    int run();

private:
    [[nodiscard]] bool loadCatalogs(const PreviewOptions &options, QString *error);
    void printCatalog() const;
    [[nodiscard]] bool loadWindow(const PreviewOptions &options);
    [[nodiscard]] bool initializeIcons(QString *error);
    void startCapture(const PreviewOptions &options);

    QGuiApplication &m_application;
    QindaQt::Profiles::ProfileCatalog m_profiles;
    QindaQt::Themes::ThemeCatalog m_themes;
    QQmlApplicationEngine m_engine;
    ShellDataRoots m_dataRoots;
    std::unique_ptr<ShellTokenPublisher> m_tokenPublisher;
    std::unique_ptr<ScreenshotCapture> m_capture;
};

} // namespace QindaQt::Shell
