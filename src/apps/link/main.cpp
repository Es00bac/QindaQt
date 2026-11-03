// SPDX-License-Identifier: GPL-3.0-or-later
// qindaqt-link: the QindaLink companion app. Architecture: docs/wiki/apps/qindalink.md.
#include "app_options.h"
#include "app_preferences.h"
#include "conversation_view_model.h"
#include "dbus_link_api.h"
#include "devices_view_model.h"
#include "fake_link_api.h"
#include "launcher.h"
#include "link_actions.h"
#include "notifications_view_model.h"
#include "phone_apps_view_model.h"
#include "process_runner.h"
#include "reply_view_model.h"
#include "threads_view_model.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtGui/QGuiApplication>
#include <QtGui/QIcon>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>

#include <cstdio>
#include <cstring>
#include <memory>

using namespace QindaQt::LinkApp;

namespace {

// Renders and the reply-form tests must never map a window on the user's desktop.
bool wantsGrab(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--") == 0) return false;
        if (std::strcmp(argv[i], "--grab") == 0 || std::strncmp(argv[i], "--grab=", 7) == 0) return true;
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    if (wantsGrab(argc, argv) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("qindaqt-link"));
    QGuiApplication::setApplicationDisplayName(QGuiApplication::translate("main", "QindaLink"));
    QGuiApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.Link"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("org.qindaqt.Link")));

    QString error;
    const auto parsed = parseAppOptions(QGuiApplication::arguments(), &error);
    if (!parsed) {
        std::fprintf(stderr, "qindaqt-link: %s\n", qPrintable(error));
        return 2;
    }
    const AppOptions options = *parsed;

    // AGENT-GUARD: a --fake run uses scripted data, a throwaway preferences file and a recording
    // process runner, and publishes no menu: it never reaches the session bus, the real service,
    // the user's config or other programs.
    std::unique_ptr<LinkApi> api;
    std::unique_ptr<ProcessRunner> runner;
    QTemporaryDir fakeConfig;
    QString preferencesPath = AppPreferences::defaultPath();
    if (options.fake()) {
        auto fake = std::make_unique<FakeLinkApi>(FakeLinkApi::scenarioFromName(options.fakeScenario));
        auto fakeRunner = std::make_unique<FakeProcessRunner>();
        fakeRunner->captureOutput = FakeLinkApi::inventoryJson();
        api = std::move(fake);
        runner = std::move(fakeRunner);
        preferencesPath = fakeConfig.filePath(QStringLiteral("app.json"));
    } else {
        api = std::make_unique<DbusLinkApi>(QDBusConnection::sessionBus());
        runner = std::make_unique<QProcessRunner>();
    }
    AppPreferences preferences(preferencesPath);

    DevicesViewModel devices(*api);
    ThreadsViewModel threads(*api);
    ConversationViewModel conversation(*api);
    PhoneAppsViewModel phoneApps(*api, *runner, preferences);
    NotificationsViewModel notifications(*api);
    Launcher launcher(*runner);
    std::unique_ptr<ReplyViewModel> reply;
    QObject::connect(&devices, &DevicesViewModel::deviceIdChanged, &threads, &ThreadsViewModel::setDeviceId);
    QObject::connect(&devices, &DevicesViewModel::deviceIdChanged, &conversation, &ConversationViewModel::setDeviceId);
    QObject::connect(&conversation, &ConversationViewModel::threadRead, &threads, &ThreadsViewModel::clearUnread);

    QindaQt::AppShell::ApplicationCoordinator coordinator;
    coordinator.setApplicationName(QGuiApplication::translate("main", "QindaLink"));
    if (const auto status = coordinator.replaceActions(linkActions()); !status.ok()) {
        std::fprintf(stderr, "qindaqt-link: could not publish the menu: %s\n", qPrintable(status.message));
        return 1;
    }

    const char *uri = "QindaQt.Link";
    qmlRegisterSingletonInstance(uri, 1, 0, "Devices", &devices);
    qmlRegisterSingletonInstance(uri, 1, 0, "Threads", &threads);
    qmlRegisterSingletonInstance(uri, 1, 0, "Conversation", &conversation);
    qmlRegisterSingletonInstance(uri, 1, 0, "PhoneApps", &phoneApps);
    qmlRegisterSingletonInstance(uri, 1, 0, "Mutes", &notifications);
    qmlRegisterSingletonInstance(uri, 1, 0, "Launcher", &launcher);
    qmlRegisterSingletonInstance(uri, 1, 0, "Prefs", &preferences);
    if (options.mode == Mode::Reply) {
        reply = std::make_unique<ReplyViewModel>(*api, options.device, options.key, options.action);
        qmlRegisterSingletonInstance(uri, 1, 0, "Reply", reply.get());
    }

    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("initialPage"), options.page);
    qml.rootContext()->setContextProperty(QStringLiteral("coordinator"), &coordinator);
    qml.loadFromModule(uri, options.mode == Mode::Reply ? "ReplyWindow" : "Main");
    if (qml.rootObjects().isEmpty()) return 1;

    auto *window = qobject_cast<QQuickWindow *>(qml.rootObjects().constFirst());
    const QPointer<QQuickWindow> guard(window);
    if (window != nullptr && options.size.isValid()) window->resize(options.size);

    std::unique_ptr<QObject> menuExport;
    if (window != nullptr && !options.fake() && options.mode == Mode::Main) {
        // The desktop hosts the menu when a registrar is there; `inWindowMenuVisible` tells QML to
        // stop drawing its own so the menu is never in two places.
        menuExport = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
            coordinator, *window, QDBusConnection::sessionBus(),
            [guard](bool visible) { if (guard) guard->setProperty("inWindowMenuVisible", visible); });
    }

    devices.refresh();
    if (options.mode == Mode::Reply) reply->load();
    if (options.page == QLatin1String("notifications") || options.fake()) notifications.refresh();

    if (options.openThread >= 0) {
        // Render aid: open the conversation once the thread list has loaded.
        auto opened = std::make_shared<bool>(false);
        QObject::connect(&threads, &ThreadsViewModel::stateChanged, &app, [&, opened] {
            const QVariantMap row = threads.thread(options.openThread);
            if (!*opened && !row.isEmpty()) {
                *opened = true;
                conversation.openThread(row);
            }
        });
    }

    if (options.headless()) {
        if (window == nullptr) { std::fprintf(stderr, "qindaqt-link: --grab needs a window root\n"); return 1; }
        QTimer::singleShot(options.grabAfterMs, window, [window, path = options.grabPath] {
            const QImage frame = window->grabWindow();
            if (frame.isNull() || !frame.save(path)) {
                std::fprintf(stderr, "qindaqt-link: could not write %s\n", qPrintable(path));
                QCoreApplication::exit(1);
                return;
            }
            std::fprintf(stderr, "wrote %s (%dx%d)\n", qPrintable(path), frame.width(), frame.height());
            QCoreApplication::quit();
        });
    }
    const int status = QGuiApplication::exec();
    menuExport.reset();
    return status;
}
