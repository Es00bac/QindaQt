// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_agent_appearance.h"
#include "polkit_dialog_view_model.h"
#include "polkit_listener.h"
#include "polkit_overlay_surface.h"
#include "polkit_requester_resolver.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <PolkitQt1/Subject>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>

#include <cstdio>
#include <unistd.h>

namespace {

using namespace QindaQt::Apps::PolkitAgent;

void addImportPaths(QQmlApplicationEngine &engine)
{
    const QString current = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString build = QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH)).canonicalFilePath();
    if (!build.isEmpty() && current == build) {
        engine.addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    }
    engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("QindaQt"));
    application.setOrganizationDomain(QStringLiteral("qindaqt.org"));
    application.setApplicationName(QStringLiteral("qindaqt-polkit-agent"));

    PolkitRequesterResolver requesterResolver(
        QStringLiteral("/proc"),
        QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation));
    PolkitListener listener(requesterResolver);
    const PolkitQt1::UnixSessionSubject subject(static_cast<qint64>(::getpid()));
    if (!listener.registerListener(subject, QStringLiteral("/org/qindaqt/PolkitAgent"))) {
        // AGENT-CONTRACT: exit code 2 means "another agent already holds
        // this session's polkit registration." session_process_supervisor's
        // OptionalSessionChild gives this child exactly one restart before
        // giving up for the rest of the supervised session (see the
        // AGENT-NOTE at its m_polkitAgent->start() call site) -- a second
        // exit(2) in a row therefore ends this child quietly rather than
        // looping, and the session continues without an authentication
        // agent rather than fighting the one that is already registered.
        std::fprintf(stderr,
                     "qindaqt-polkit-agent: could not register with polkitd for this "
                     "session (another agent is already registered)\n");
        return 2;
    }

    QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport(
        QDBusConnection::sessionBus());
    QindaQt::Services::SettingsClient::SettingsClient settingsClient(
        settingsTransport,
        {QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme"),
         QStringLiteral("appearance.iconTheme")});
    QString settingsError;
    if (!settingsClient.start(&settingsError)) {
        qWarning("qindaqt-polkit-agent: Settings1 unavailable: %s", qPrintable(settingsError));
    }
    QindaQt::AppAppearance::ApplicationAppearanceController appearance(
        settingsClient, QindaQt::AppAppearance::standardThemeDirectories({}),
        QStringLiteral("qinda-dark"));

    QQmlApplicationEngine engine;
    addImportPaths(engine);
    QString appearanceError;
    auto *facade = ensurePolkitAgentTokenFacade(engine, &appearanceError);
    if (facade == nullptr
        || !applyPolkitAgentAppearance(appearance, *facade, application, &appearanceError)) {
        std::fprintf(stderr, "qindaqt-polkit-agent: %s\n", qPrintable(appearanceError));
        return 3;
    }
    QObject::connect(
        &appearance, &QindaQt::AppAppearance::ApplicationAppearanceController::appearanceChanged,
        &engine, [&appearance, facade, &application] {
            QString error;
            if (!applyPolkitAgentAppearance(appearance, *facade, application, &error)) {
                qWarning("qindaqt-polkit-agent: appearance update rejected: %s",
                        qPrintable(error));
            }
        });

    PolkitDialogViewModel viewModel;
    viewModel.observe(listener.queue());
    engine.rootContext()->setContextProperty(QStringLiteral("polkitViewModel"), &viewModel);
    engine.loadFromModule(QStringLiteral("QindaQt.PolkitAgentApp"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 4;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        std::fprintf(stderr, "qindaqt-polkit-agent: dialog root is not a window\n");
        return 4;
    }
    PolkitOverlaySurface::configure(*window);
    QObject::connect(&viewModel, &PolkitDialogViewModel::visibleChanged, window,
                     [window, &viewModel] { window->setVisible(viewModel.isVisible()); });

    return application.exec();
}
