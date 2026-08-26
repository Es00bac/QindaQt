// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellpreviewapplication.h"

#include "screenshotcapture.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTextStream>

namespace QindaQt::Shell {

ShellPreviewApplication::ShellPreviewApplication(QGuiApplication &application)
    : m_application(application)
{
    connect(&m_engine, &QQmlApplicationEngine::objectCreationFailed, &m_application, [] {
        QCoreApplication::exit(3);
    });
}

ShellPreviewApplication::~ShellPreviewApplication() = default;

int ShellPreviewApplication::run()
{
    const PreviewOptionsResult result = parsePreviewOptions(m_application);
    if (!result.options.has_value()) {
        qCritical().noquote() << result.error;
        return 2;
    }
    const PreviewOptions &options = *result.options;

    QString error;
    if (!loadCatalogs(options, &error)) {
        qCritical().noquote() << error;
        return 2;
    }
    if (options.listOnly) {
        printCatalog();
        return 0;
    }
    if (!loadWindow(options)) {
        return 3;
    }
    if (options.capturesScreenshot()) {
        startCapture(options);
    }
    return m_application.exec();
}

QString ShellPreviewApplication::resolveDataDirectory(const QString &explicitPath,
                                                      const char *environmentName,
                                                      const char *sourcePath,
                                                      const QString &installedSuffix) const
{
    if (!explicitPath.isEmpty()) {
        return QDir::cleanPath(explicitPath);
    }
    const QString environmentPath = qEnvironmentVariable(environmentName);
    if (!environmentPath.isEmpty()) {
        return QDir::cleanPath(environmentPath);
    }
    if (QDir(QString::fromUtf8(sourcePath)).exists()) {
        return QString::fromUtf8(sourcePath);
    }
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  installedSuffix,
                                  QStandardPaths::LocateDirectory);
}

bool ShellPreviewApplication::loadCatalogs(const PreviewOptions &options, QString *error)
{
    const QString profileDirectory = resolveDataDirectory(options.profileDirectory,
                                                          "QINDAQT_PROFILE_DIR",
                                                          QINDAQT_SOURCE_PROFILE_DIR,
                                                          QStringLiteral("qindaqt/profiles"));
    const QString themeDirectory = resolveDataDirectory(options.themeDirectory,
                                                        "QINDAQT_THEME_DIR",
                                                        QINDAQT_SOURCE_THEME_DIR,
                                                        QStringLiteral("qindaqt/themes"));
    if (!m_profiles.loadDirectory(profileDirectory, error)) {
        return false;
    }
    if (!m_themes.loadDirectory(themeDirectory, error)) {
        return false;
    }
    if (!m_profiles.selectById(options.profileId)) {
        *error = QStringLiteral("Unknown profile: %1").arg(options.profileId);
        return false;
    }

    const QString requestedTheme = !options.themeId.isEmpty()
        ? options.themeId
        : m_profiles.current().value(QStringLiteral("defaultTheme")).toString();
    if (!m_themes.selectById(requestedTheme)) {
        *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
        return false;
    }
    return true;
}

void ShellPreviewApplication::printCatalog() const
{
    QTextStream output(stdout);
    output << "Profiles:\n";
    for (const auto &profile : m_profiles.profiles()) {
        output << "  " << profile.id << " - " << profile.name << '\n';
    }
    output << "Themes:\n";
    for (const auto &theme : m_themes.themes()) {
        output << "  " << theme.id << " - " << theme.name << '\n';
    }
}

bool ShellPreviewApplication::loadWindow(const PreviewOptions &options)
{
    m_engine.rootContext()->setContextProperty(QStringLiteral("profileCatalog"), &m_profiles);
    m_engine.rootContext()->setContextProperty(QStringLiteral("themeCatalog"), &m_themes);
    m_engine.rootContext()->setContextProperty(QStringLiteral("requestedPreviewWidth"), options.width);
    m_engine.rootContext()->setContextProperty(QStringLiteral("requestedPreviewHeight"), options.height);
    m_engine.loadFromModule(QStringLiteral("QindaQt.Shell"), QStringLiteral("Main"));
    return !m_engine.rootObjects().isEmpty();
}

void ShellPreviewApplication::startCapture(const PreviewOptions &options)
{
    auto *window = qobject_cast<QQuickWindow *>(m_engine.rootObjects().constFirst());
    if (window == nullptr) {
        qCritical() << "Shell preview root object is not a QQuickWindow";
        QCoreApplication::exit(3);
        return;
    }

    m_capture = std::make_unique<ScreenshotCapture>(options.screenshotPath,
                                                    QSize(options.width, options.height),
                                                    this);
    connect(m_capture.get(), &ScreenshotCapture::finished, &m_application,
            [](bool succeeded, const QString &message) {
                if (succeeded) {
                    qInfo().noquote() << "Saved screenshot:" << message;
                } else {
                    qCritical().noquote() << message;
                }
                QCoreApplication::exit(succeeded ? 0 : 4);
            });
    m_capture->start(*window);
}

} // namespace QindaQt::Shell
