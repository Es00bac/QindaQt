// SPDX-License-Identifier: GPL-3.0-or-later
#include "voice_console_model.h"

#include <qindaqt/services/voice_client/qt_voice_transport.h>
#include <qindaqt/services/voice_client/voice_client.h>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-voice"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Voice"));
    QCoreApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.Voice"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaQt voice dictation console (GPL-3.0-or-later)"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);
    if (!parser.positionalArguments().isEmpty()) {
        std::fprintf(stderr, "qindaqt-voice: this application takes no arguments\n");
        return 2;
    }

    // AGENT-CONTRACT: the transport is the only D-Bus in this process. The
    // model and QML see the client's bounded projection and nothing else.
    QindaQt::Services::Voice::QtVoiceTransport transport(QDBusConnection::sessionBus());
    QindaQt::Services::Voice::VoiceClient client(&transport);
    QindaQt::Apps::Voice::VoiceConsoleModel model(client);

    QQmlApplicationEngine engine;
    // AGENT-GUARD: the singleton is registered before the module is loaded and
    // is owned by this scope, not by QML. Letting QML own it would destroy the
    // model while the client is still delivering a completion.
    qmlRegisterSingletonInstance("QindaQt.Voice", 1, 0, "VoiceConsole", &model);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("QindaQt.VoiceConsole", "Main");
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "qindaqt-voice: the console window could not be created\n");
        return 1;
    }

    client.start();
    return application.exec();
}
