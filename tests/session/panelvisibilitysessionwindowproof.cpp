// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitysessionwindowproof.h"

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointF>
#include <QTextStream>
#include <QThread>

#include <functional>

namespace QindaQt::Test::PanelVisibilityWindowProof {
namespace {

QJsonObject call(QDBusInterface &endpoint, const QString &method,
                 const QByteArray &argument = {})
{
    const QDBusMessage reply = argument.isEmpty()
        ? endpoint.call(method) : endpoint.call(method, argument);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().size() != 1) {
        return {};
    }
    const QJsonDocument document =
        QJsonDocument::fromJson(reply.arguments().constFirst().toByteArray());
    return document.isObject() ? document.object() : QJsonObject{};
}

QJsonArray surfaces(QDBusInterface &endpoint)
{
    const auto result = call(endpoint, QStringLiteral("DevelopmentShellSurfaces"));
    return result.value(QStringLiteral("status")) == QStringLiteral("ok")
        ? result.value(QStringLiteral("surfaces")).toArray() : QJsonArray{};
}

bool mappedLeftPanel(const QJsonArray &items)
{
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        const QJsonObject geometry = item.value(QStringLiteral("geometry")).toObject();
        if (geometry.value(QStringLiteral("x")).toInt() == 0
            && geometry.value(QStringLiteral("width")).toInt() == 40
            && item.value(QStringLiteral("mapped")).toBool()
            && item.value(QStringLiteral("committed")).toBool()) {
            return true;
        }
    }
    return false;
}

QJsonObject pointer(double x, double y)
{
    return {{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
            {QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}

QJsonObject key(bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("key")},
            {QStringLiteral("key"), QStringLiteral("left-meta")},
            {QStringLiteral("pressed"), pressed}};
}

QJsonObject button(bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("button")},
            {QStringLiteral("button"), QStringLiteral("left")},
            {QStringLiteral("pressed"), pressed}};
}

bool inject(QDBusInterface &endpoint, QJsonArray events)
{
    const QJsonObject request{{QStringLiteral("schemaVersion"), 1},
                              {QStringLiteral("events"), std::move(events)}};
    const auto result = call(
        endpoint, QStringLiteral("InjectTestInput"),
        QJsonDocument(request).toJson(QJsonDocument::Compact));
    return result.value(QStringLiteral("status")) == QStringLiteral("injected");
}

std::optional<QRect> waitForProofWindowGeometry(QDBusInterface &endpoint)
{
    QElapsedTimer timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        const auto frame = proofWindowGeometry(endpoint);
        if (frame && !frame->isEmpty()) {
            return frame;
        }
        QThread::msleep(20);
    } while (timer.elapsed() < 5'000);
    return std::nullopt;
}

std::optional<WindowMoveProof> dragProofWindow(
    QDBusInterface &endpoint, const QPointF &targetCenter,
    const std::function<bool(const QRect &)> &accept)
{
    const auto frame = waitForProofWindowGeometry(endpoint);
    if (!frame) {
        return std::nullopt;
    }
    const QPointF start(frame->center());
    if (!inject(endpoint, {pointer(start.x(), start.y())})) {
        return std::nullopt;
    }
    QThread::msleep(30);
    if (!inject(endpoint, {key(true)}) || !inject(endpoint, {button(true)})) {
        return std::nullopt;
    }
    const QPointF primed(start.x(), start.y() + 48.0);
    if (!inject(endpoint, {pointer(primed.x(), primed.y())})) {
        return std::nullopt;
    }
    QThread::msleep(30);
    constexpr int Steps = 12;
    for (int step = 1; step <= Steps; ++step) {
        const qreal progress = qreal(step) / qreal(Steps);
        const QPointF position = primed + ((targetCenter - primed) * progress);
        if (!inject(endpoint, {pointer(position.x(), position.y())})) {
            return std::nullopt;
        }
        QThread::msleep(15);
    }
    if (!inject(endpoint, {button(false)}) || !inject(endpoint, {key(false)})) {
        return std::nullopt;
    }
    const QRect before = *frame;
    QElapsedTimer movedTimer;
    movedTimer.start();
    std::optional<QRect> lastAfter;
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        const auto after = proofWindowGeometry(endpoint);
        lastAfter = after;
        if (after && !after->isEmpty() && *after != before && accept(*after)) {
            return WindowMoveProof{before, *after, {}};
        }
        QThread::msleep(20);
    } while (movedTimer.elapsed() < 5'000);
    QTextStream(stderr)
        << "drag did not establish geometry: before=" << before.x() << ','
        << before.y() << ',' << before.width() << 'x' << before.height()
        << " after=" << (lastAfter ? lastAfter->x() : -1) << ','
        << (lastAfter ? lastAfter->y() : -1) << ','
        << (lastAfter ? lastAfter->width() : -1) << 'x'
        << (lastAfter ? lastAfter->height() : -1) << '\n';
    return std::nullopt;
}

} // namespace

std::optional<QRect> proofWindowGeometry(QDBusInterface &endpoint)
{
    const QJsonObject result = call(endpoint, QStringLiteral("Windows"));
    for (const QJsonValue &value : result.value(QStringLiteral("windows")).toArray()) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("title"))
            != QStringLiteral("QindaQt panel visibility proof client")) {
            continue;
        }
        const QJsonObject geometry = item.value(QStringLiteral("geometry")).toObject();
        return QRect(geometry.value(QStringLiteral("x")).toInt(),
                     geometry.value(QStringLiteral("y")).toInt(),
                     geometry.value(QStringLiteral("width")).toInt(),
                     geometry.value(QStringLiteral("height")).toInt());
    }
    return std::nullopt;
}

bool waitForMaximizedProofWindowGeometry(QDBusInterface &endpoint,
                                         const QRect &output)
{
    QElapsedTimer timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        const auto frame = proofWindowGeometry(endpoint);
        if (frame && frame->width() > output.width() / 2
            && frame->height() > output.height() / 2
            && frame->height() < output.height()) {
            return true;
        }
        QThread::msleep(20);
    } while (timer.elapsed() < 5'000);
    return false;
}

QJsonObject geometryObject(const QRect &geometry)
{
    return {{QStringLiteral("x"), geometry.x()},
            {QStringLiteral("y"), geometry.y()},
            {QStringLiteral("width"), geometry.width()},
            {QStringLiteral("height"), geometry.height()}};
}

std::optional<WindowMoveProof> moveProofWindowFromPanel(
    QDBusInterface &endpoint, const QRect &output)
{
    // AGENT-NOTE: P1-4 requires hidden authority at the final pre-input
    // boundary; an earlier fullscreen transition is not evidence of this.
    const auto current = waitForProofWindowGeometry(endpoint);
    if (!current) {
        return std::nullopt;
    }
    const QJsonArray authorityBefore = surfaces(endpoint);
    if (mappedLeftPanel(authorityBefore)) {
        return std::nullopt;
    }
    const QPointF target(current->center().x(), output.height() * 0.55);
    auto proof = dragProofWindow(endpoint, target, [output](const QRect &after) {
        return QRect(QPoint{}, output.size()).contains(after)
            && after.width() < output.width() && after.height() < output.height();
    });
    if (proof) {
        proof->authorityBefore = authorityBefore;
    }
    return proof;
}

std::optional<WindowMoveProof> restoreProofWindowOntoPanel(
    QDBusInterface &endpoint, const QRect &output)
{
    const QRect leftStrip(0, 0, 40, output.height());
    return dragProofWindow(
        endpoint, QPointF(output.width() * 0.68, output.height() * 0.50),
        [leftStrip, output](const QRect &after) {
            return after.intersects(leftStrip) && after.width() < output.width()
                && after.height() < output.height();
        });
}

} // namespace QindaQt::Test::PanelVisibilityWindowProof
