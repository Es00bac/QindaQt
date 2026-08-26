// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositorprobeclient.h"

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QThread>

#include <cmath>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ObjectPath = "/org/qindaqt/Compositor";
constexpr auto InterfaceName = "org.qindaqt.Compositor1";

std::optional<QRectF> parseFrame(const QJsonValue &value, QLatin1StringView field, QString *error)
{
    if (!value.isObject()) {
        *error = QStringLiteral("Windows returned malformed %1").arg(field);
        return std::nullopt;
    }
    const auto rectangle = value.toObject();
    for (const auto &component : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"),
                                  QStringLiteral("height")}) {
        if (!rectangle.value(component).isDouble()) {
            *error = QStringLiteral("Windows returned malformed %1").arg(field);
            return std::nullopt;
        }
    }
    return QRectF(rectangle.value(QStringLiteral("x")).toDouble(),
                  rectangle.value(QStringLiteral("y")).toDouble(),
                  rectangle.value(QStringLiteral("width")).toDouble(),
                  rectangle.value(QStringLiteral("height")).toDouble());
}

std::optional<QJsonObject> decode(const QDBusReply<QByteArray> &reply, const QString &operation,
                                  QString *error)
{
    if (!reply.isValid()) {
        *error = QStringLiteral("%1 D-Bus call failed: %2").arg(operation, reply.error().message());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error =
            QStringLiteral("%1 returned invalid JSON: %2").arg(operation, parseError.errorString());
        return std::nullopt;
    }
    return document.object();
}

std::optional<ObservedWindow> parseWindow(const QJsonObject &object, QString *error)
{
    const auto frame = object.value(QStringLiteral("geometry"));
    const auto id = object.value(QStringLiteral("id"));
    const auto title = object.value(QStringLiteral("title"));
    const auto owner = object.value(QStringLiteral("containerId"));
    const auto minimized = object.value(QStringLiteral("minimized"));
    const auto active = object.value(QStringLiteral("active"));
    const auto skipTaskbar = object.value(QStringLiteral("skipTaskbar"));
    const auto skipSwitcher = object.value(QStringLiteral("skipSwitcher"));
    const auto keepAbove = object.value(QStringLiteral("keepAbove"));
    const auto keepBelow = object.value(QStringLiteral("keepBelow"));
    const auto stackIndex = object.value(QStringLiteral("stackIndex"));
    const auto targetFrame = object.value(QStringLiteral("targetGeometry"));
    const auto serverDecorated = object.value(QStringLiteral("serverDecorated"));
    const auto decorationClass = object.value(QStringLiteral("decorationClass"));
    if (!id.isString() || id.toString().isEmpty() || !title.isString() || !owner.isString() ||
        !minimized.isBool() || !active.isBool() || !skipTaskbar.isBool()
        || !skipSwitcher.isBool() || !keepAbove.isBool() || !keepBelow.isBool()
        || !stackIndex.isDouble()
        || !serverDecorated.isBool() || !decorationClass.isString()) {
        *error = QStringLiteral("Windows returned a malformed window entry");
        return std::nullopt;
    }
    const auto parsedFrame = parseFrame(frame, QLatin1StringView("geometry"), error);
    const auto parsedTarget = parseFrame(targetFrame, QLatin1StringView("targetGeometry"), error);
    if (!parsedFrame || !parsedTarget) {
        return std::nullopt;
    }
    return ObservedWindow{
        .id = id.toString(),
        .title = title.toString(),
        .containerId = owner.toString(),
        .frame = *parsedFrame,
        .targetFrame = *parsedTarget,
        .minimized = minimized.toBool(),
        .active = active.toBool(),
        .skipTaskbar = skipTaskbar.toBool(),
        .skipSwitcher = skipSwitcher.toBool(),
        .keepAbove = keepAbove.toBool(),
        .keepBelow = keepBelow.toBool(),
        .stackIndex = stackIndex.toInt(-1),
        .serverDecorated = serverDecorated.toBool(),
        .decorationClass = decorationClass.toString(),
    };
}

std::optional<WindowInventory> selectWindows(const QJsonArray &windows, const QStringList &titles,
                                             QString *error)
{
    WindowInventory selected;
    for (const auto &value : windows) {
        if (!value.isObject()) {
            *error = QStringLiteral("Windows returned a non-object entry");
            return std::nullopt;
        }
        auto parsed = parseWindow(value.toObject(), error);
        if (!parsed) {
            return std::nullopt;
        }
        if (titles.contains(parsed->title)) {
            selected.insert(parsed->title, std::move(*parsed));
        }
    }
    return selected;
}

} // namespace

CompositorProbeClient::CompositorProbeClient()
    : m_endpoint(std::make_unique<QDBusInterface>(QString::fromLatin1(ServiceName),
                                                  QString::fromLatin1(ObjectPath),
                                                  QString::fromLatin1(InterfaceName)))
{
}

CompositorProbeClient::~CompositorProbeClient() = default;

std::optional<QJsonObject> CompositorProbeClient::call(const QString &method, QString *error)
{
    error->clear();
    return decode(m_endpoint->call(method), method, error);
}

std::optional<QJsonObject> CompositorProbeClient::call(const QString &method,
                                                       const QVariant &argument, QString *error)
{
    error->clear();
    return decode(m_endpoint->call(method, argument), method, error);
}

std::optional<QJsonObject> CompositorProbeClient::callWithArguments(const QString &method,
                                                                    const QVariantList &arguments,
                                                                    QString *error)
{
    error->clear();
    const QDBusReply<QByteArray> reply =
        m_endpoint->callWithArgumentList(QDBus::Block, method, arguments);
    return decode(reply, method, error);
}

std::optional<QJsonArray> CompositorProbeClient::arrayReply(const QString &method,
                                                            QLatin1StringView field, QString *error)
{
    const auto reply = call(method, error);
    if (!reply) {
        return std::nullopt;
    }
    if (reply->value(QStringLiteral("status")) != QStringLiteral("ok") ||
        !reply->value(field).isArray()) {
        *error = QStringLiteral("%1 returned an unexpected response").arg(method);
        return std::nullopt;
    }
    return reply->value(field).toArray();
}

std::optional<QJsonArray> CompositorProbeClient::windows(QString *error)
{
    return arrayReply(QStringLiteral("Windows"), QLatin1StringView("windows"), error);
}

std::optional<QJsonArray> CompositorProbeClient::outputs(QString *error)
{
    return arrayReply(QStringLiteral("Outputs"), QLatin1StringView("outputs"), error);
}

std::optional<QJsonArray> CompositorProbeClient::containers(QString *error)
{
    return arrayReply(QStringLiteral("Containers"), QLatin1StringView("containers"), error);
}

std::optional<WindowInventory>
CompositorProbeClient::awaitWindows(const QStringList &titles,
                                    const std::function<bool(const WindowInventory &)> &ready,
                                    QString *error, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    QJsonArray lastInventory;
    QString lastError;
    while (timer.elapsed() < timeoutMilliseconds) {
        // AGENT-NOTE: KWin moveResize emits Wayland configures. Processing the
        // client event loop is required before inventory can reflect geometry
        // acknowledged by all painted probe windows.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QString inventoryError;
        const auto listed = windows(&inventoryError);
        if (listed) {
            lastInventory = *listed;
            auto selected = selectWindows(*listed, titles, &inventoryError);
            if (selected && selected->size() == titles.size() && ready(*selected)) {
                error->clear();
                return selected;
            }
        }
        if (!inventoryError.isEmpty()) {
            lastError = inventoryError;
        }
        QThread::msleep(10);
    }
    *error = lastError.isEmpty()
                 ? QStringLiteral("timed out waiting for window state; inventory=%1")
                       .arg(QString::fromUtf8(
                           QJsonDocument(lastInventory).toJson(QJsonDocument::Compact)))
                 : lastError;
    return std::nullopt;
}

std::optional<QJsonObject> CompositorProbeClient::dock(const QString &targetWindowId,
                                                       const QString &incomingWindowId,
                                                       double ratio, QString *error)
{
    return callWithArguments(QStringLiteral("DockWindows"),
                             {targetWindowId, incomingWindowId, QStringLiteral("horizontal"),
                              QStringLiteral("second"), ratio},
                             error);
}

bool nearlyEqual(qreal first, qreal second)
{
    return std::abs(first - second) <= 1.0;
}

bool sameGeometry(const QRectF &first, const QRectF &second)
{
    return nearlyEqual(first.x(), second.x()) && nearlyEqual(first.y(), second.y()) &&
           nearlyEqual(first.width(), second.width()) &&
           nearlyEqual(first.height(), second.height());
}

} // namespace QindaQt::Test
