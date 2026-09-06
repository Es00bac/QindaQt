// SPDX-License-Identifier: GPL-3.0-or-later
#include "welcome_actions.h"
#include "welcome_appearance.h"
#include "welcome_preferences.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>

#include <cstdio>

namespace {

void addWelcomeImportPaths(QQmlApplicationEngine &engine) {
    const QString current = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString build = QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH)).canonicalFilePath();
    if (!build.isEmpty() && current == build) {
        engine.addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    }
    engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
}

QString optionalHeroSource() {
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                          QStringLiteral("qindaqt/wallpapers/qinda-punk.png"),
                                          QStandardPaths::LocateFile);
    if (path.isEmpty()) {
        const QString besideExecutable = QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_WALLPAPER_RELATIVE_PATH)
                                  + QStringLiteral("/qinda-punk.png"));
        if (QFileInfo::exists(besideExecutable)) {
            path = besideExecutable;
        }
    }
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

int verifyUiContract(QQmlApplicationEngine &engine, bool exerciseOptOut,
                     QindaQt::Apps::Welcome::WelcomePreferences &preferences) {
    QObject *root = engine.rootObjects().constFirst();
    QObject *checkbox = root->findChild<QObject *>(QStringLiteral("showAtNextLaunch"));
    if (checkbox == nullptr
        || root->findChild<QObject *>(QStringLiteral("previousButton")) == nullptr
        || root->findChild<QObject *>(QStringLiteral("nextButton")) == nullptr
        || root->property("width").toInt() != 900
        || root->property("height").toInt() != 640
        || root->property("minimumWidth").toInt() != 640
        || root->property("minimumHeight").toInt() != 480) {
        std::fprintf(stderr, "qindaqt-welcome: incomplete UI contract\n");
        return 5;
    }
    if (exerciseOptOut) {
        checkbox->setProperty("checked", false);
        QCoreApplication::processEvents();
        if (preferences.showAtNextLaunch()) {
            std::fprintf(stderr, "qindaqt-welcome: checkbox did not persist opt-out\n");
            return 5;
        }
    }
    std::printf("welcome-ui-ready show-next=%s\n",
                preferences.showAtNextLaunch() ? "true" : "false");
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("QindaQt"));
    application.setOrganizationDomain(QStringLiteral("qindaqt.org"));
    application.setApplicationName(QStringLiteral("qindaqt-welcome"));
    application.setApplicationDisplayName(QStringLiteral("Welcome to QindaQt"));
    application.setDesktopFileName(QStringLiteral("org.qindaqt.Welcome"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QindaQt desktop guide"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("first-launch"),
                      QStringLiteral("Open only when the welcome preference permits it")});
    parser.addOption({QStringLiteral("theme-directory"),
                      QStringLiteral("Additional local theme directory"),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("check-ui-contract"),
                      QStringLiteral("Construct and verify the tutorial UI, then exit")});
    parser.addOption({QStringLiteral("exercise-opt-out"),
                      QStringLiteral("Uncheck the tutorial preference while verifying the UI")});
    parser.process(application);

    QindaQt::Apps::Welcome::WelcomePreferences preferences;
    if (parser.isSet(QStringLiteral("first-launch"))
        && !preferences.showAtNextLaunch()) {
        return 0;
    }

    QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport(
        QDBusConnection::sessionBus());
    QindaQt::Services::SettingsClient::SettingsClient settingsClient(
        settingsTransport,
        {QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme")});
    QString settingsError;
    if (!settingsClient.start(&settingsError)) {
        qWarning("qindaqt-welcome: Settings1 unavailable: %s", qPrintable(settingsError));
    }

    QindaQt::AppAppearance::ApplicationAppearanceController appearance(
        settingsClient,
        QindaQt::AppAppearance::standardThemeDirectories(
            parser.value(QStringLiteral("theme-directory"))),
        QStringLiteral("qinda-dark"));
    QindaQt::Apps::Welcome::WelcomeActions actions;
    QQmlApplicationEngine engine;
    addWelcomeImportPaths(engine);

    QString appearanceError;
    auto *facade = QindaQt::Apps::Welcome::ensureWelcomeTokenFacade(engine, &appearanceError);
    if (facade == nullptr
        || !QindaQt::Apps::Welcome::applyWelcomeAppearance(
            appearance, *facade, application, &appearanceError)) {
        std::fprintf(stderr, "qindaqt-welcome: %s\n", qPrintable(appearanceError));
        return 3;
    }
    QObject::connect(
        &appearance, &QindaQt::AppAppearance::ApplicationAppearanceController::appearanceChanged,
        &engine, [&appearance, facade, &application] {
            QString error;
            if (!QindaQt::Apps::Welcome::applyWelcomeAppearance(
                    appearance, *facade, application, &error)) {
                qWarning("qindaqt-welcome: appearance update rejected: %s", qPrintable(error));
            }
        });

    engine.rootContext()->setContextProperty(QStringLiteral("welcomePreferences"),
                                              &preferences);
    engine.rootContext()->setContextProperty(QStringLiteral("welcomeActions"), &actions);
    engine.rootContext()->setContextProperty(QStringLiteral("welcomeHeroSource"),
                                              optionalHeroSource());
    engine.loadFromModule(QStringLiteral("QindaQt.WelcomeApp"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 4;
    }
    if (parser.isSet(QStringLiteral("check-ui-contract"))) {
        return verifyUiContract(
            engine, parser.isSet(QStringLiteral("exercise-opt-out")), preferences);
    }
    return application.exec();
}
