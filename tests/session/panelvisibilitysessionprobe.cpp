// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitycaptureprocess.h"
#include "panelvisibilitycontrolchannel.h"
#include "panelvisibilityprobearguments.h"
#include "panelvisibilityphasewaiter.h"
#include "panelvisibilitysessionwindowproof.h"

#include <QBackingStore>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QElapsedTimer>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QWindow>

#include <algorithm>
#include <functional>
#include <optional>

namespace {

using namespace QindaQt::Test::PanelVisibilityPhase;

constexpr auto Service = "org.qindaqt.Compositor";
constexpr auto Path = "/org/qindaqt/Compositor";
constexpr auto Interface = "org.qindaqt.Compositor1";

class PaintedWindow final : public QWindow {
public:
    explicit PaintedWindow(QString title) : m_store(this)
    {
        setTitle(std::move(title));
        setFlags(Qt::Window);
        resize(720, 480);
    }

protected:
    void exposeEvent(QExposeEvent *event) override
    {
        QWindow::exposeEvent(event);
        paint();
    }
    void resizeEvent(QResizeEvent *event) override
    {
        QWindow::resizeEvent(event);
        paint();
    }

private:
    void paint()
    {
        if (!isExposed() || size().isEmpty()) {
            return;
        }
        m_store.resize(size());
        const QRegion region(QRect(QPoint{}, size()));
        m_store.beginPaint(region);
        QPainter painter(m_store.paintDevice());
        painter.fillRect(region.boundingRect(), QColor(QStringLiteral("#197f91")));
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 20));
        painter.drawText(region.boundingRect(), Qt::AlignCenter,
                         QStringLiteral("Panel visibility proof"));
        painter.end();
        m_store.endPaint();
        m_store.flush(region);
    }
    QBackingStore m_store;
};

QJsonObject call(QDBusInterface &endpoint, const QString &method,
                 const QByteArray &argument = {})
{
    const QDBusMessage reply = argument.isEmpty()
        ? endpoint.call(method) : endpoint.call(method, argument);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().size() != 1) {
        return {};
    }
    const QByteArray bytes = reply.arguments().constFirst().toByteArray();
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    return document.isObject() ? document.object() : QJsonObject{};
}

class CompositorSurfaceAuthority final : public SurfaceAuthority {
public:
    explicit CompositorSurfaceAuthority(QDBusInterface &endpoint)
        : m_endpoint(endpoint)
    {
    }

    QJsonArray snapshot() override
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        const QJsonObject result = call(
            m_endpoint, QStringLiteral("DevelopmentShellSurfaces"));
        return result.value(QStringLiteral("status")) == QStringLiteral("ok")
            ? result.value(QStringLiteral("surfaces")).toArray() : QJsonArray{};
    }

private:
    QDBusInterface &m_endpoint;
};

class DeadlinePollTimer final : public PollTimer {
public:
    explicit DeadlinePollTimer(int timeoutMilliseconds)
        : m_timeoutMilliseconds(timeoutMilliseconds)
    {
        m_elapsed.start();
    }

    bool expired() const override
    {
        return m_elapsed.elapsed() >= m_timeoutMilliseconds;
    }

    void waitForNextPoll() override { QThread::msleep(20); }

private:
    QElapsedTimer m_elapsed;
    int m_timeoutMilliseconds;
};

bool waitFor(QDBusInterface &endpoint, const std::function<bool(const QJsonArray &)> &ready,
             const QSize &output, QJsonArray *observed,
             int timeoutMilliseconds = 5'000)
{
    CompositorSurfaceAuthority authority(endpoint);
    DeadlinePollTimer timer(timeoutMilliseconds);
    return waitForSettledPhase(authority, timer, output, ready, observed);
}

bool inject(QDBusInterface &endpoint, QJsonArray events)
{
    const QJsonObject request{{QStringLiteral("schemaVersion"), 1},
                              {QStringLiteral("events"), std::move(events)}};
    const QJsonObject result = call(
        endpoint, QStringLiteral("InjectTestInput"),
        QJsonDocument(request).toJson(QJsonDocument::Compact));
    return result.value(QStringLiteral("status")) == QStringLiteral("injected");
}

QJsonObject pointer(double x, double y);
QJsonObject key(const char *name, bool pressed);

QJsonObject pointer(double x, double y)
{
    return {{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
            {QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}

QJsonObject key(const char *name, bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("key")},
            {QStringLiteral("key"), QString::fromLatin1(name)},
            {QStringLiteral("pressed"), pressed}};
}

bool capture(const QString &tool, const QString &libraryPath,
             const QString &phase, QString *failure)
{
    QDir directory(QStringLiteral("/var/lib/qindaqt-evidence"));
    const QStringList beforeList = directory.entryList(
        {QStringLiteral("wayland-screenshot-*.png")}, QDir::Files);
    const QSet<QString> before(beforeList.cbegin(), beforeList.cend());
    QProcess process;
    if (!QindaQt::Test::PanelVisibilityCapture::configureCaptureProcess(
            process, QProcessEnvironment::systemEnvironment(), tool,
            libraryPath, failure)) {
        return false;
    }
    process.setWorkingDirectory(directory.path());
    process.start();
    if (!process.waitForStarted(5'000)) {
        *failure = QStringLiteral("could not start: %1").arg(process.errorString());
        return false;
    }
    if (!process.waitForFinished(5'000)) {
        process.kill();
        process.waitForFinished();
        *failure = QStringLiteral("timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString output = QString::fromUtf8(
            process.readAllStandardError() + process.readAllStandardOutput()).trimmed();
        *failure = QStringLiteral("exited %1: %2").arg(process.exitCode()).arg(output);
        return false;
    }
    const QStringList after = directory.entryList(
        {QStringLiteral("wayland-screenshot-*.png")}, QDir::Files);
    QString created;
    for (const QString &candidate : after) {
        if (!before.contains(candidate)) {
            if (!created.isEmpty()) {
                *failure = QStringLiteral("created more than one fresh PNG");
                return false;
            }
            created = candidate;
        }
    }
    if (created.isEmpty()) {
        *failure = QStringLiteral("created no fresh PNG");
        return false;
    }
    if (!directory.rename(created, QStringLiteral("panel-%1.png").arg(phase))) {
        *failure = QStringLiteral("could not name the phase PNG");
        return false;
    }
    return true;
}

bool requirePhase(QDBusInterface &endpoint, QJsonArray *items,
                  const std::function<bool(const QJsonArray &)> &ready,
                  const QSize &output,
                  const QString &captureTool, const QString &captureLibraryPath,
                  const QString &phase,
                  QJsonArray *evidence)
{
    if (!waitFor(endpoint, ready, output, items)) {
        const auto visibility = call(
            endpoint, QStringLiteral("ShellVisibilitySnapshot"));
        QTextStream(stderr)
            << "panel authority phase failed: " << phase << '\n'
            << "last surface authority: "
            << QJsonDocument(*items).toJson(QJsonDocument::Compact) << '\n'
            << "last visibility authority: "
            << QJsonDocument(visibility).toJson(QJsonDocument::Compact) << '\n';
        return false;
    }
    QString captureFailure;
    if (!capture(captureTool, captureLibraryPath, phase, &captureFailure)) {
        QTextStream(stderr) << "panel capture failed: " << phase << ": "
                            << captureFailure << '\n';
        return false;
    }
    evidence->append(QJsonObject{{QStringLiteral("phase"), phase},
                                 {QStringLiteral("surfaces"), *items}});
    return true;
}


bool waitForFullscreenState(PaintedWindow &client, bool fullscreen)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (client.windowState() == (fullscreen ? Qt::WindowFullScreen : Qt::WindowNoState)) {
            return true;
        }
        QThread::msleep(20);
    }
    return false;
}

bool runFullscreenControl(PaintedWindow &client, const QString &controlFile)
{
    using namespace QindaQt::Test::PanelVisibilityControl;

    Channel channel(controlFile);
    QString failure;
    if (!channel.isValid(&failure) || !channel.publishReady(client.title(), &failure)) {
        QTextStream(stderr) << "panel control setup failed: " << failure << '\n';
        return false;
    }

    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < 45'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        const std::optional<Command> command = channel.readNext(&failure);
        if (!failure.isEmpty()) {
            QTextStream(stderr) << "panel control command failed: " << failure << '\n';
            return false;
        }
        if (!command.has_value()) {
            QThread::msleep(20);
            continue;
        }
        if (command->action == Action::Close) {
            client.close();
            if (!channel.acknowledge(*command, false, &failure)) {
                QTextStream(stderr) << "panel control acknowledgement failed: " << failure << '\n';
                return false;
            }
            return true;
        }

        // AGENT-GUARD: Do not call requestActivate here. The runtime owner uses
        // its own real drag and focus transitions around this client; commands
        // may change fullscreen state but must not steal focus from that flow.
        if (command->fullscreen) {
            client.showFullScreen();
        } else {
            client.showNormal();
        }
        if (!waitForFullscreenState(client, command->fullscreen)
            || !channel.acknowledge(*command, client.windowState() == Qt::WindowFullScreen, &failure)) {
            QTextStream(stderr) << "panel fullscreen control failed: " << failure << '\n';
            return false;
        }
    }
    if (!channel.acknowledgeTimeout(&failure)) {
        QTextStream(stderr) << "panel control timeout acknowledgement failed: " << failure << '\n';
    }
    QTextStream(stderr) << "panel control timed out waiting for close" << '\n';
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    using namespace QindaQt::Test::PanelVisibilityWindowProof;

    QGuiApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    PanelVisibilityProbe::Arguments probeArguments;
    QString argumentError;
    if (!PanelVisibilityProbe::parseArguments(application.arguments(), &probeArguments,
                                              &argumentError)
        || application.screens().size() != 1) {
        return 2;
    }
    auto *const bus = QDBusConnection::sessionBus().interface();
    if (bus == nullptr) {
        return 3;
    }
    QElapsedTimer serviceTimer;
    serviceTimer.start();
    while (serviceTimer.elapsed() < 15'000
           && !bus->isServiceRegistered(QString::fromLatin1(Service)).value()) {
        QThread::msleep(20);
    }
    QDBusInterface endpoint(QString::fromLatin1(Service), QString::fromLatin1(Path),
                            QString::fromLatin1(Interface));
    const QRect output = application.primaryScreen()->geometry();
    const QSize outputSize = output.size();
    QJsonArray observed;
    QJsonArray phases;
    const auto topVisible = [&](const QJsonArray &items) {
        return mappedPanel(items, QStringLiteral("top"), 30, outputSize, 30);
    };
    if (!waitFor(endpoint, topVisible, outputSize, &observed, 15'000)) {
        return 4;
    }
    // A unique title is opt-in for the private fullscreen qualifier, which
    // needs two independently addressable client-control endpoints. The
    // ordinary panel-visibility scenario retains its stable default title.
    PaintedWindow client(probeArguments.title);
    client.showFullScreen();
    client.requestActivate();
    const auto overlapHidden = [&](const QJsonArray &items) {
        return topVisible(items)
            && !mappedPanel(items, QStringLiteral("left"), 40, outputSize);
    };
    const QString captureTool = probeArguments.captureTool;
    const QString captureLibraryPath = probeArguments.captureLibraryPath;
    const QString controlFile = probeArguments.controlFile;
    if (!requirePhase(endpoint, &observed, overlapHidden, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("window-overlap-hidden"), &phases)) {
        return 5;
    }
    client.showNormal();
    client.showMaximized();
    client.requestActivate();
    if (!waitForMaximizedProofWindowGeometry(endpoint, output)) {
        return 6;
    }
    const auto restoredOntoPanel = restoreProofWindowOntoPanel(endpoint, output);
    if (!restoredOntoPanel) {
        return 6;
    }
    // Clear the client's maximized request after the compositor-side restore.
    // The authority is sampled again below, so this normalization cannot stand
    // in for the required hidden-immediately-before-input proof.
    client.showNormal();
    client.requestActivate();
    if (!waitFor(endpoint, overlapHidden, outputSize, &observed)) {
        return 6;
    }
    const auto movedAway = moveProofWindowFromPanel(endpoint, output);
    if (!movedAway) {
        return 6;
    }
    const auto leftVisible = [&](const QJsonArray &items) {
        return topVisible(items)
            && mappedPanel(items, QStringLiteral("left"), 40, outputSize, 40);
    };
    if (!requirePhase(endpoint, &observed, leftVisible, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("window-moved-away"), &phases)) {
        return 6;
    }
    if (hasControlFile) {
        return runFullscreenControl(client, controlFile) ? 0 : 18;
    }
    client.showFullScreen();
    client.requestActivate();
    if (!requirePhase(endpoint, &observed, overlapHidden, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("window-close-hidden"), &phases)) {
        return 7;
    }
    const auto closeGeometry = proofWindowGeometry(endpoint);
    if (!closeGeometry) {
        return 7;
    }
    client.close();
    if (!requirePhase(endpoint, &observed, leftVisible, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("window-closed-restored"), &phases)) {
        return 7;
    }
    const double centerX = static_cast<double>(output.width()) / 2.0;
    const double bottomY = static_cast<double>(output.height() - 1);
    if (!inject(endpoint, {pointer(centerX, bottomY)})) {
        return 8;
    }
    const auto bottomVisible = [&](const QJsonArray &items) {
        return topVisible(items)
            && mappedPanel(items, QStringLiteral("bottom"), 48, outputSize);
    };
    if (!requirePhase(endpoint, &observed, bottomVisible, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("edge-revealed"), &phases)) {
        return 9;
    }
    if (!inject(endpoint, {pointer(centerX, output.height() / 2.0)})) {
        return 10;
    }
    const auto bottomHidden = [&](const QJsonArray &items) {
        return topVisible(items)
            && !mappedPanel(items, QStringLiteral("bottom"), 48, outputSize);
    };
    if (!waitFor(endpoint, bottomHidden, outputSize, &observed)) {
        return 11;
    }
    if (!inject(endpoint, {key("left-meta", true), key("space", true),
                           key("space", false), key("left-meta", false)})) {
        return 12;
    }
    if (!requirePhase(endpoint, &observed, bottomVisible, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("shortcut-revealed"), &phases)) {
        return 13;
    }
    if (!inject(endpoint, {key("left-meta", true), key("n", true),
                           key("n", false), key("left-meta", false)})) {
        return 14;
    }
    if (!inject(endpoint, {pointer(centerX, output.height() / 2.0)})) {
        return 14;
    }
    QThread::msleep(1'700);
    if (!requirePhase(endpoint, &observed, bottomVisible, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("popup-held"), &phases)) {
        return 15;
    }
    if (!inject(endpoint, {key("left-meta", true), key("n", true),
                           key("n", false), key("left-meta", false)})) {
        return 16;
    }
    if (!requirePhase(endpoint, &observed, bottomHidden, outputSize, captureTool,
                      captureLibraryPath,
                      QStringLiteral("popup-closed"), &phases)) {
        return 17;
    }
    const QJsonObject result{{QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("outputWidth"), output.width()},
                             {QStringLiteral("outputHeight"), output.height()},
                             {QStringLiteral("move"),
                              QJsonObject{
                                  {QStringLiteral("before"),
                                   geometryObject(movedAway->before)},
                                  {QStringLiteral("after"),
                                   geometryObject(movedAway->after)},
                                  {QStringLiteral("surfacesBefore"),
                                   movedAway->authorityBefore}}},
                             {QStringLiteral("close"),
                              QJsonObject{
                                  {QStringLiteral("before"),
                                   geometryObject(*closeGeometry)},
                                  {QStringLiteral("windowAbsentAfter"),
                                   !proofWindowGeometry(endpoint).has_value()}}},
                             {QStringLiteral("phases"), phases}};
    QTextStream(stdout) << "QINDAQT_PANEL_VISIBILITY_EVIDENCE="
                        << QJsonDocument(result).toJson(QJsonDocument::Compact)
                        << '\n';
    return 0;
}
