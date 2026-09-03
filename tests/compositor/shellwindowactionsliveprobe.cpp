// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"
#include "shellwindowactionsliveclients.h"

#include <LayerShellQt/Window>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QThread>
#include <QWindow>

#include <functional>
#include <optional>
#include <utility>

using QindaQt::Compositor::ShellWindowAction;
using QindaQt::Compositor::ShellWindowActionStatus;
using QindaQt::Compositor::ShellWindowGeneration;
using QindaQt::Compositor::ShellWindowIdentityStatus;
using QindaQt::Compositor::TestSupport::ShellWindowActionsLiveClientMarker;
using QindaQt::Compositor::TestSupport::isFailClosedUnauthorizedIdentityReply;
using QindaQt::Compositor::TestSupport::parseLiveClientNativeWindowId;
using QindaQt::Compositor::TestSupport::runShellWindowActionsLiveWindow;
using QindaQt::Compositor::TestSupport::runUnauthorizedIdentityClient;
using QindaQt::ShellWindowActionsClient::QtShellWindowActionsTransport;
using QindaQt::ShellWindowActionsClient::ShellWindowActionsClient;

namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ControlPath = "/org/qindaqt/Compositor";
constexpr auto ControlInterface = "org.qindaqt.Compositor1";
constexpr auto ShellPath = "/org/qindaqt/CompositorShell";
constexpr auto ShellInterface = "org.qindaqt.CompositorShell1";
constexpr auto Marker = "QINDAQT_SHELL_WINDOW_ACTIONS_LIVE=";
constexpr auto FirstTitle = "QindaQt action probe one";
constexpr auto SecondTitle = "QindaQt action probe two";
constexpr int TimeoutMilliseconds = 12'000;
QString inventoryDiagnostic;

struct Inventory final {
    ShellWindowGeneration generation;
    QJsonArray windows;
};

void emitResult(bool passed, const QString &failure = {})
{
    const QJsonObject result{{QStringLiteral("passed"), passed},
                             {QStringLiteral("failure"), failure}};
    QTextStream(stdout) << Marker
                        << QJsonDocument(result).toJson(QJsonDocument::Compact)
                        << Qt::endl;
}

bool waitFor(const std::function<bool()> &predicate, int timeout = TimeoutMilliseconds)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (predicate()) {
            return true;
        }
        QThread::msleep(10);
    }
    return false;
}

std::optional<Inventory> readInventory()
{
    QDBusInterface interface(QString::fromLatin1(ServiceName),
                             QString::fromLatin1(ControlPath),
                             QString::fromLatin1(ControlInterface),
                             QDBusConnection::sessionBus());
    const QDBusReply<QByteArray> reply = interface.call(QStringLiteral("Windows"));
    if (!reply.isValid()) {
        inventoryDiagnostic = reply.error().message();
        return std::nullopt;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(reply.value(), &error);
    const auto object = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject()
        || object.value(QStringLiteral("status")).toString() != QStringLiteral("ok")
        || !object.value(QStringLiteral("generationAvailable")).toBool()) {
        inventoryDiagnostic = QStringLiteral("invalid or unavailable Windows response: %1")
                                  .arg(QString::fromUtf8(reply.value()));
        return std::nullopt;
    }
    bool revisionOk = false;
    const auto revision = object.value(QStringLiteral("revision"))
                              .toString().toULongLong(&revisionOk);
    Inventory inventory{{object.value(QStringLiteral("epoch")).toString(), revision},
                        object.value(QStringLiteral("windows")).toArray()};
    if (!revisionOk || !inventory.generation.isValid()) {
        inventoryDiagnostic = QStringLiteral("Windows generation was malformed");
        return std::nullopt;
    }
    inventoryDiagnostic.clear();
    return inventory;
}

std::optional<QJsonObject> windowByTitle(const Inventory &inventory,
                                         const QString &title)
{
    for (const auto &value : inventory.windows) {
        const auto window = value.toObject();
        if (window.value(QStringLiteral("title")).toString() == title) {
            return window;
        }
    }
    return std::nullopt;
}

QString methodName(ShellWindowAction action)
{
    switch (action) {
    case ShellWindowAction::Activate: return QStringLiteral("ActivateWindow");
    case ShellWindowAction::Minimize: return QStringLiteral("MinimizeWindow");
    case ShellWindowAction::Unminimize: return QStringLiteral("UnminimizeWindow");
    case ShellWindowAction::Close: return QStringLiteral("CloseWindow");
    case ShellWindowAction::Raise: return QStringLiteral("RaiseWindow");
    }
    return {};
}

int runUnauthorized(const QStringList &arguments)
{
    if (arguments.size() != 5) {
        return 2;
    }
    QDBusInterface interface(QString::fromLatin1(ServiceName),
                             QString::fromLatin1(ShellPath),
                             QString::fromLatin1(ShellInterface),
                             QDBusConnection::sessionBus());
    const QDBusReply<QByteArray> reply = interface.call(
        QStringLiteral("ActivateWindow"), arguments[2], arguments[3], arguments[4]);
    if (!reply.isValid()) {
        return 3;
    }
    QTextStream(stdout) << reply.value() << Qt::endl;
    return 0;
}

int runHostileUnauthorized()
{
    const QString hostile(1024 * 1024, u'9');
    QDBusInterface interface(QString::fromLatin1(ServiceName),
                             QString::fromLatin1(ShellPath),
                             QString::fromLatin1(ShellInterface),
                             QDBusConnection::sessionBus());
    const QDBusReply<QByteArray> reply = interface.call(
        QStringLiteral("ActivateWindow"), hostile, hostile, hostile);
    if (!reply.isValid()) {
        return 3;
    }
    QTextStream(stdout) << reply.value() << Qt::endl;
    return 0;
}

class LiveProof final {
public:
    explicit LiveProof(QGuiApplication &application)
        : m_application(application)
        , m_transport(QDBusConnection::sessionBus())
        , m_client(m_transport)
    {
    }

    int run()
    {
        if (!mapPanel() || !startClients()) {
            return finish(false, QStringLiteral("could not map the test clients"));
        }
        Inventory inventory;
        if (!waitFor([&] {
                const auto current = readInventory();
                if (!current || !windowByTitle(*current, QString::fromLatin1(FirstTitle))
                    || !windowByTitle(*current, QString::fromLatin1(SecondTitle))) {
                    return false;
                }
                inventory = *current;
                return true;
            })) {
            QString details = inventoryDiagnostic;
            for (auto *process : std::as_const(m_clients)) {
                const QString output = QString::fromUtf8(process->readAllStandardError()).trimmed();
                if (!output.isEmpty()) details += QStringLiteral(" child: %1").arg(output);
            }
            return finish(false,
                          QStringLiteral("ordinary windows were not inventoried: %1")
                              .arg(details));
        }
        const auto first = *windowByTitle(inventory, QString::fromLatin1(FirstTitle));
        const QString firstId = first.value(QStringLiteral("id")).toString();
        if (!proveUnauthorized(firstId, inventory.generation)) {
            return finish(false, m_failure);
        }
        QString error;
        if (!m_client.start(&error)
            || !waitFor([this] { return m_client.available(); })) {
            return finish(false, QStringLiteral("action client unavailable: %1").arg(error));
        }
        if (!actAndObserve(ShellWindowAction::Activate, FirstTitle,
                           [](const QJsonObject &window, const Inventory &) {
                               return window.value(QStringLiteral("active")).toBool();
                           })
            || !proveIdentity(FirstTitle, 0, false)
            || !actAndObserve(ShellWindowAction::Minimize, FirstTitle,
                              [](const QJsonObject &window, const Inventory &) {
                                  return window.value(QStringLiteral("minimized")).toBool();
                              })
            || !actAndObserve(ShellWindowAction::Unminimize, FirstTitle,
                              [](const QJsonObject &window, const Inventory &) {
                                  return !window.value(QStringLiteral("minimized")).toBool();
                              })
            || !actAndObserve(ShellWindowAction::Activate, SecondTitle,
                              [](const QJsonObject &window, const Inventory &) {
                                  return window.value(QStringLiteral("active")).toBool();
                              })
            || !proveIdentity(SecondTitle, 1, true)) {
            return finish(false, m_failure);
        }
        if (!actAndObserve(ShellWindowAction::Raise, FirstTitle,
                           [](const QJsonObject &window, const Inventory &current) {
                               const auto other = windowByTitle(
                                   current, QString::fromLatin1(SecondTitle));
                               return other && window.value(QStringLiteral("stackIndex")).toInt()
                                   > other->value(QStringLiteral("stackIndex")).toInt();
                           })) {
            return finish(false, m_failure);
        }
        if (!actAndObserve(ShellWindowAction::Close, SecondTitle,
                           [](const QJsonObject &, const Inventory &) { return true; }, true)) {
            return finish(false, m_failure);
        }
        return finish(true);
    }

private:
    using Observation = std::function<bool(const QJsonObject &, const Inventory &)>;

    bool mapPanel()
    {
        m_panel.setTitle(QStringLiteral("QindaQt authenticated action panel"));
        m_panel.setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        auto *const layer = LayerShellQt::Window::get(&m_panel);
        if (!layer) {
            return false;
        }
        layer->setScope(QStringLiteral("dock"));
        layer->setLayer(LayerShellQt::Window::LayerTop);
        LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
        anchors |= LayerShellQt::Window::AnchorLeft;
        anchors |= LayerShellQt::Window::AnchorRight;
        layer->setAnchors(anchors);
        layer->setDesiredSize(QSize(0, 24));
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityNone);
        m_panel.show();
        return true;
    }

    bool startClients()
    {
        const QString executable = m_application.applicationFilePath();
        const QList<QString> titles{QString::fromLatin1(FirstTitle),
                                    QString::fromLatin1(SecondTitle)};
        for (qsizetype index = 0; index < titles.size(); ++index) {
            const QString &title = titles[index];
            auto *process = new QProcess(&m_application);
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                               index == 0 ? QStringLiteral("wayland")
                                          : QStringLiteral("xcb"));
            process->setProcessEnvironment(environment);
            process->setProgram(executable);
            process->setArguments({QStringLiteral("--window"), title});
            process->start();
            if (!process->waitForStarted(2000)) {
                return false;
            }
            QByteArray output;
            if (!waitFor([&] {
                    output += process->readAllStandardOutput();
                    return output.contains(ShellWindowActionsLiveClientMarker);
                }, 5000)) {
                return false;
            }
            const auto nativeId = parseLiveClientNativeWindowId(output, title);
            if (!nativeId) {
                return false;
            }
            m_clients.append(process);
            m_nativeWindowIds.append(*nativeId);
        }
        return true;
    }

    bool proveUnauthorized(const QString &windowId,
                           const ShellWindowGeneration &generation)
    {
        QProcess attacker;
        attacker.setProgram(m_application.applicationFilePath());
        attacker.setArguments({QStringLiteral("--unauthorized"), windowId,
                               generation.epoch, QString::number(generation.revision)});
        attacker.start();
        if (!attacker.waitForFinished(3000) || attacker.exitCode() != 0) {
            m_failure = QStringLiteral("unbound caller failed: %1")
                            .arg(QString::fromUtf8(attacker.readAllStandardError()));
            return false;
        }
        const QByteArray output = attacker.readAllStandardOutput().trimmed();
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(output, &error);
        const bool rejected = error.error == QJsonParseError::NoError
            && document.object().value(QStringLiteral("status")).toString()
                == QStringLiteral("unauthorized");
        if (!rejected) {
            m_failure = QStringLiteral("unbound caller response was %1")
                            .arg(QString::fromUtf8(output));
        }
        return rejected && proveHostileUnauthorized()
            && proveUnauthorizedIdentity();
    }

    bool proveHostileUnauthorized()
    {
        QProcess attacker;
        attacker.setProgram(m_application.applicationFilePath());
        attacker.setArguments({QStringLiteral("--unauthorized-hostile")});
        attacker.start();
        if (!attacker.waitForFinished(5000) || attacker.exitCode() != 0) {
            m_failure = QStringLiteral("wrong-PID hostile-size stage failed: %1")
                            .arg(QString::fromUtf8(attacker.readAllStandardError()));
            return false;
        }
        const QByteArray output = attacker.readAllStandardOutput().trimmed();
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(output, &error);
        const auto object = document.object();
        const bool rejected = error.error == QJsonParseError::NoError
            && document.isObject() && output.size() < 512
            && object.value(QStringLiteral("status")).toString()
                == QStringLiteral("unauthorized")
            && object.value(QStringLiteral("failure")).toObject()
                   .value(QStringLiteral("code")).toString()
                == QStringLiteral("caller-pid-mismatch")
            && !object.contains(QStringLiteral("windowId"))
            && !object.contains(QStringLiteral("epoch"))
            && !object.contains(QStringLiteral("revision"));
        if (!rejected) {
            m_failure = QStringLiteral(
                "wrong-PID hostile-size reply was not bounded and echo-free: %1")
                            .arg(QString::fromUtf8(output.left(1024)));
        }
        return rejected;
    }

    bool proveUnauthorizedIdentity()
    {
        QProcess attacker;
        attacker.setProgram(m_application.applicationFilePath());
        attacker.setArguments({QStringLiteral("--unauthorized-identity")});
        attacker.start();
        if (!attacker.waitForFinished(3000) || attacker.exitCode() != 0) {
            m_failure = QStringLiteral("unbound identity caller failed");
            return false;
        }
        const QByteArray output = attacker.readAllStandardOutput().trimmed();
        const bool rejected = isFailClosedUnauthorizedIdentityReply(output);
        if (!rejected) {
            m_failure = QStringLiteral("unbound identity response leaked facts: %1")
                            .arg(QString::fromUtf8(output.left(1024)));
        }
        return rejected;
    }

    bool proveIdentity(const char *title, qsizetype clientIndex,
                       bool expectX11WindowId)
    {
        const QString expectedTitle = QString::fromLatin1(title);
        const qint64 expectedPid = m_clients[clientIndex]->processId();
        const quint64 expectedNativeId = m_nativeWindowIds[clientIndex];
        const bool observed = waitFor([&] {
            const auto inventory = readInventory();
            const auto window = inventory
                ? windowByTitle(*inventory, expectedTitle) : std::nullopt;
            const auto &snapshot = m_client.identitySnapshot();
            if (!inventory || !window || !snapshot
                || snapshot->status != ShellWindowIdentityStatus::Ok
                || !snapshot->activeWindow
                || snapshot->actionGeneration != inventory->generation) {
                return false;
            }
            const auto &identity = *snapshot->activeWindow;
            const bool windowIdMatches = identity.windowId
                == window->value(QStringLiteral("id")).toString();
            const bool processMatches = identity.processId
                && *identity.processId == expectedPid;
            const bool menuIdMatches = expectX11WindowId
                ? identity.appMenuWindowId
                    && static_cast<quint64>(*identity.appMenuWindowId)
                        == expectedNativeId
                : !identity.appMenuWindowId;
            return windowIdMatches && processMatches && menuIdMatches;
        });
        if (!observed) {
            m_failure = QStringLiteral(
                "active identity did not match %1 expected pid=%2 menu=%3")
                .arg(expectedTitle, QString::number(expectedPid),
                     expectX11WindowId ? QString::number(expectedNativeId)
                                       : QStringLiteral("null"));
        }
        return observed;
    }

    bool actAndObserve(ShellWindowAction action, const char *title,
                       const Observation &observation, bool expectGone = false)
    {
        const auto inventory = readInventory();
        const auto window = inventory
            ? windowByTitle(*inventory, QString::fromLatin1(title)) : std::nullopt;
        if (!inventory || !window) {
            m_failure = QStringLiteral("target inventory unavailable for %1")
                            .arg(methodName(action));
            return false;
        }
        const QString windowId = window->value(QStringLiteral("id")).toString();
        const quint64 previousToken = m_client.lastResult()
            ? m_client.lastResult()->token : 0;
        QString error;
        if (!m_client.request(action, windowId, inventory->generation, &error)
            || !waitFor([&] {
                   return m_client.lastResult()
                       && m_client.lastResult()->token > previousToken;
               })) {
            m_failure = QStringLiteral("%1 did not reply: %2")
                            .arg(methodName(action), error);
            return false;
        }
        const auto &outcome = *m_client.lastResult();
        if (!outcome.serverResult || outcome.serverResult->status
            != ShellWindowActionStatus::Admitted) {
            m_failure = QStringLiteral("%1 was not admitted")
                            .arg(methodName(action));
            return false;
        }
        if (!waitFor([&] {
                const auto current = readInventory();
                if (!current) return false;
                const auto currentWindow = windowByTitle(*current,
                                                         QString::fromLatin1(title));
                return expectGone ? !currentWindow
                                  : currentWindow && observation(*currentWindow, *current);
            })) {
            m_failure = QStringLiteral("%1 effect was not observed")
                            .arg(methodName(action));
            return false;
        }
        return true;
    }

    int finish(bool passed, const QString &failure = {})
    {
        m_client.stop();
        for (auto *process : std::as_const(m_clients)) {
            if (process->state() != QProcess::NotRunning) {
                process->terminate();
                if (!process->waitForFinished(1000)) process->kill();
            }
        }
        m_panel.hide();
        emitResult(passed, failure);
        return passed ? 0 : 1;
    }

    QGuiApplication &m_application;
    QWindow m_panel;
    QList<QProcess *> m_clients;
    QList<quint64> m_nativeWindowIds;
    QtShellWindowActionsTransport m_transport;
    ShellWindowActionsClient m_client;
    QString m_failure;
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() >= 3 && arguments[1] == QStringLiteral("--window")) {
        return runShellWindowActionsLiveWindow(application, arguments[2]);
    }
    if (arguments.size() >= 2 && arguments[1] == QStringLiteral("--unauthorized")) {
        return runUnauthorized(arguments);
    }
    if (arguments.size() == 2
        && arguments[1] == QStringLiteral("--unauthorized-hostile")) {
        return runHostileUnauthorized();
    }
    if (arguments.size() == 2
        && arguments[1] == QStringLiteral("--unauthorized-identity")) {
        return runUnauthorizedIdentityClient();
    }
    return LiveProof(application).run();
}
