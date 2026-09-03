// SPDX-License-Identifier: GPL-3.0-or-later
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

constexpr auto Service = "org.qindaqt.Compositor";
constexpr auto Path = "/org/qindaqt/Compositor";
constexpr auto Interface = "org.qindaqt.Compositor1";

class PaintedWindow final : public QWindow {
public:
    PaintedWindow() : m_store(this)
    {
        setTitle(QStringLiteral("QindaQt panel visibility proof client"));
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

QJsonArray surfaces(QDBusInterface &endpoint)
{
    const QJsonObject result = call(endpoint, QStringLiteral("DevelopmentShellSurfaces"));
    return result.value(QStringLiteral("status")) == QStringLiteral("ok")
        ? result.value(QStringLiteral("surfaces")).toArray() : QJsonArray{};
}

bool mappedPanel(const QJsonArray &items, const QString &edge, int thickness,
                 int outputWidth, int outputHeight, int zone = -2)
{
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        const QJsonObject geometry = item.value(QStringLiteral("geometry")).toObject();
        const int x = geometry.value(QStringLiteral("x")).toInt();
        const int y = geometry.value(QStringLiteral("y")).toInt();
        const int width = geometry.value(QStringLiteral("width")).toInt();
        const int height = geometry.value(QStringLiteral("height")).toInt();
        const bool position =
            (edge == QStringLiteral("top") && y == 0 && height == thickness)
            || (edge == QStringLiteral("left") && x == 0 && width == thickness)
            || (edge == QStringLiteral("bottom") && height == thickness
                && y + height == outputHeight && x >= 0
                && x + width <= outputWidth);
        if (position && item.value(QStringLiteral("mapped")).toBool()
            && item.value(QStringLiteral("committed")).toBool()
            && (zone == -2
                || item.value(QStringLiteral("exclusiveZone")).toInt(-999) == zone)) {
            return true;
        }
    }
    return false;
}

bool waitFor(QDBusInterface &endpoint, const std::function<bool(const QJsonArray &)> &ready,
             QJsonArray *observed, int timeoutMilliseconds = 5'000)
{
    QElapsedTimer timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        *observed = surfaces(endpoint);
        if (ready(*observed)) {
            return true;
        }
        QThread::msleep(20);
    } while (timer.elapsed() < timeoutMilliseconds);
    return false;
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

bool capture(const QString &tool, const QString &phase)
{
    QDir directory(QStringLiteral("/var/lib/qindaqt-evidence"));
    const QStringList beforeList = directory.entryList(
        {QStringLiteral("wayland-screenshot-*.png")}, QDir::Files);
    const QSet<QString> before(beforeList.cbegin(), beforeList.cend());
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"),
                       QStringLiteral("qindaqt-parent-wayland"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(directory.path());
    process.start(tool);
    if (!process.waitForFinished(5'000) || process.exitCode() != 0) {
        return false;
    }
    const QStringList after = directory.entryList(
        {QStringLiteral("wayland-screenshot-*.png")}, QDir::Files);
    QString created;
    for (const QString &candidate : after) {
        if (!before.contains(candidate)) {
            if (!created.isEmpty()) {
                return false;
            }
            created = candidate;
        }
    }
    return !created.isEmpty()
        && directory.rename(created, QStringLiteral("panel-%1.png").arg(phase));
}

bool requirePhase(QDBusInterface &endpoint, QJsonArray *items,
                  const std::function<bool(const QJsonArray &)> &ready,
                  const QString &captureTool, const QString &phase,
                  QJsonArray *evidence)
{
    if (!waitFor(endpoint, ready, items) || !capture(captureTool, phase)) {
        QTextStream(stderr) << "panel phase failed: " << phase << '\n';
        return false;
    }
    evidence->append(QJsonObject{{QStringLiteral("phase"), phase},
                                 {QStringLiteral("surfaces"), *items}});
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    using namespace QindaQt::Test::PanelVisibilityWindowProof;

    QGuiApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    if (application.arguments().size() != 2 || application.screens().size() != 1) {
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
    QJsonArray observed;
    QJsonArray phases;
    const auto topVisible = [&](const QJsonArray &items) {
        return mappedPanel(items, QStringLiteral("top"), 30,
                           output.width(), output.height(), 30);
    };
    if (!waitFor(endpoint, topVisible, &observed, 15'000)) {
        return 4;
    }
    PaintedWindow client;
    client.showFullScreen();
    client.requestActivate();
    const auto overlapHidden = [&](const QJsonArray &items) {
        return topVisible(items)
            && !mappedPanel(items, QStringLiteral("left"), 40,
                            output.width(), output.height());
    };
    const QString captureTool = application.arguments().at(1);
    if (!requirePhase(endpoint, &observed, overlapHidden, captureTool,
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
    if (!waitFor(endpoint, overlapHidden, &observed)) {
        return 6;
    }
    const auto movedAway = moveProofWindowFromPanel(endpoint, output);
    if (!movedAway) {
        return 6;
    }
    const auto leftVisible = [&](const QJsonArray &items) {
        return topVisible(items)
            && mappedPanel(items, QStringLiteral("left"), 40,
                           output.width(), output.height(), 40);
    };
    if (!requirePhase(endpoint, &observed, leftVisible, captureTool,
                      QStringLiteral("window-moved-away"), &phases)) {
        return 6;
    }
    client.showFullScreen();
    client.requestActivate();
    if (!requirePhase(endpoint, &observed, overlapHidden, captureTool,
                      QStringLiteral("window-close-hidden"), &phases)) {
        return 7;
    }
    const auto closeGeometry = proofWindowGeometry(endpoint);
    if (!closeGeometry) {
        return 7;
    }
    client.close();
    if (!requirePhase(endpoint, &observed, leftVisible, captureTool,
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
            && mappedPanel(items, QStringLiteral("bottom"), 48,
                           output.width(), output.height());
    };
    if (!requirePhase(endpoint, &observed, bottomVisible, captureTool,
                      QStringLiteral("edge-revealed"), &phases)) {
        return 9;
    }
    if (!inject(endpoint, {pointer(centerX, output.height() / 2.0)})) {
        return 10;
    }
    const auto bottomHidden = [&](const QJsonArray &items) {
        return topVisible(items)
            && !mappedPanel(items, QStringLiteral("bottom"), 48,
                            output.width(), output.height());
    };
    if (!waitFor(endpoint, bottomHidden, &observed)) {
        return 11;
    }
    if (!inject(endpoint, {key("left-meta", true), key("space", true),
                           key("space", false), key("left-meta", false)})) {
        return 12;
    }
    if (!requirePhase(endpoint, &observed, bottomVisible, captureTool,
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
    if (!requirePhase(endpoint, &observed, bottomVisible, captureTool,
                      QStringLiteral("popup-held"), &phases)) {
        return 15;
    }
    if (!inject(endpoint, {key("left-meta", true), key("n", true),
                           key("n", false), key("left-meta", false)})) {
        return 16;
    }
    if (!requirePhase(endpoint, &observed, bottomHidden, captureTool,
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
