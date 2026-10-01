// SPDX-License-Identifier: GPL-3.0-or-later
#include "consent_controller.h"
#include <qindaqt/platform/foreign_parent/foreign_parent.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
using namespace QindaQt::Services::Portal;
namespace {
std::optional<AccessQuestion> question() {
    QByteArray bytes; QElapsedTimer timer; timer.start();
    while (bytes.size() <= 131072) {
        const auto remaining = 2000 - timer.elapsed(); pollfd input{STDIN_FILENO, POLLIN, 0};
        if (remaining <= 0 || poll(&input, 1, static_cast<int>(remaining)) <= 0) return {};
        char buffer[4096]; const auto count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count < 0) return {};
        if (count == 0) break;
        bytes.append(buffer, static_cast<qsizetype>(count));
    }
    if (bytes.size() > 131072) return {};
    QJsonParseError error; const auto object = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError || object.size() != 10) return {};
    const auto s = [&object](const char *key) { return object.value(QLatin1String(key)).toString(); };
    QVariantMap options{{QStringLiteral("deny_label"), s("deny")}, {QStringLiteral("grant_label"), s("grant")},
        {QStringLiteral("icon"), s("icon")}, {QStringLiteral("modal"), object.value(QStringLiteral("modal")).toBool()}};
    AccessChoices choices;
    for (const auto &value : object.value(QStringLiteral("choices")).toArray()) {
        const auto row = value.toObject(); AccessChoice choice;
        choice.id = row.value(QStringLiteral("id")).toString(); choice.label = row.value(QStringLiteral("label")).toString();
        choice.initial = row.value(QStringLiteral("initial")).toString();
        for (const auto &item : row.value(QStringLiteral("options")).toArray()) {
            const auto option = item.toObject();
            choice.options.append({option.value(QStringLiteral("id")).toString(), option.value(QStringLiteral("label")).toString()});
        }
        choices.append(choice);
    }
    options.insert(QStringLiteral("choices"), QVariant::fromValue(choices));
    return accessQuestion(s("app"), s("parent"), s("title"), s("subtitle"), s("body"), options);
}
}
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2;
    signal(SIGPIPE, SIG_IGN);
    if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty()) return 2;
    QGuiApplication app(argc, argv);
    const auto request = question(); if (!request) return 2;
    ConsentController controller(*request);
    QQmlApplicationEngine engine; engine.rootContext()->setContextProperty(QStringLiteral("consent"), &controller);
    engine.loadFromModule(QStringLiteral("QindaQt.PortalConsent"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); if (!window) return 2;
    window->setModality(request->modal ? Qt::ApplicationModal : Qt::NonModal);
    QindaQt::Platform::ForeignParent::ForeignParent parent;
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::ready, &controller, &ConsentController::markReady);
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::lost, &controller, &ConsentController::fail);
    window->show(); parent.attach(*window, request->parentWindow);
    QTimer::singleShot(60000, &controller, &ConsentController::fail);
    return app.exec();
}
