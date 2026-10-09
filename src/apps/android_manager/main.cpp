// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/android_manager/application_controller.h"
#include "native_setup_port.h"
#include "qindaqt/services/android_runtime/native_bootstrap.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-android-applications"));
    QCoreApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QGuiApplication::setApplicationDisplayName(QObject::tr("Android Applications"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.AndroidApplications"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke"), QStringLiteral("Open briefly and exit for verification")});
    parser.process(application);

    // AGENT-GUARD: native helper admission never supplies qualified guest facts.
    // Missing package/session/recipe stays unavailable; no ambient guest starts.
    QindaQt::Services::AndroidRuntime::NativeRuntimeBootstrap bootstrap;
    static_cast<void>(bootstrap.start());
    auto setupPort = QindaQt::AndroidManager::createNativeSetupPort();
    QindaQt::AndroidManager::ApplicationController manager(bootstrap.port(), nullptr, 30000, setupPort.get(), bootstrap.error());
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("manager"), QVariant::fromValue(&manager)}});
    engine.loadFromModule(QStringLiteral("QindaQt.AndroidApplications"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (parser.isSet(QStringLiteral("smoke")))
        QTimer::singleShot(150, &application, &QCoreApplication::quit);
    return application.exec();
}
