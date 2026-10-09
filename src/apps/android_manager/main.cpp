// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/android_manager/application_controller.h"

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

    // AGENT-GUARD: No admitted production registration/runtime port exists
    // yet. Null is honest unavailable; never substitute stock app names or
    // start waydroid from QML/main to make this window appear functional.
    QindaQt::AndroidManager::ApplicationController manager;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("manager"), QVariant::fromValue(&manager)}});
    engine.loadFromModule(QStringLiteral("QindaQt.AndroidApplications"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (parser.isSet(QStringLiteral("smoke")))
        QTimer::singleShot(150, &application, &QCoreApplication::quit);
    return application.exec();
}
