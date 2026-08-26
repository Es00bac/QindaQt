// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridtestinputdriver.h"

#include "compositorprobeclient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <algorithm>

namespace QindaQt::Test {
namespace {

QJsonObject pointerEvent(const QPointF &position)
{
    return {{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
            {QStringLiteral("x"), position.x()},
            {QStringLiteral("y"), position.y()}};
}

QJsonObject keyEvent(QLatin1StringView name, bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("key")},
            {QStringLiteral("key"), name},
            {QStringLiteral("pressed"), pressed}};
}

QJsonObject buttonEvent(QLatin1StringView name, bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("button")},
            {QStringLiteral("button"), name},
            {QStringLiteral("pressed"), pressed}};
}

} // namespace

void processProbeEventsFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}

DotoolProcess::~DotoolProcess()
{
    if (m_process.state() == QProcess::Running) {
        static_cast<void>(writeCommands({QStringLiteral("buttonup left"),
                                         QStringLiteral("buttonup right"),
                                         QStringLiteral("keyup down"),
                                         QStringLiteral("keyup enter"),
                                         QStringLiteral("keyup shift"),
                                         QStringLiteral("keyup super")}, nullptr));
        m_process.closeWriteChannel();
        if (!m_process.waitForFinished(1000)) {
            m_process.terminate();
            if (!m_process.waitForFinished(500)) {
                m_process.kill();
                m_process.waitForFinished(500);
            }
        }
    }
}

bool DotoolProcess::start(const QString &program, QString *error)
{
    if (!QFileInfo(program).isExecutable()) {
        *error = QStringLiteral("dotool path is not executable: %1").arg(program);
        return false;
    }
    m_process.setProgram(program);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start(QIODevice::ReadWrite);
    if (!m_process.waitForStarted(2000)) {
        *error = QStringLiteral("could not start dotool: %1").arg(m_process.errorString());
        return false;
    }
    return true;
}

bool DotoolProcess::activateFirstContextMenuAction(
    const QPointF &point,
    const QRectF &output,
    QString *error)
{
    if (!moveTo(point, output, error)) {
        return false;
    }
    processProbeEventsFor(120);
    if (!writeCommands({QStringLiteral("buttondown right")}, error)) {
        return false;
    }
    processProbeEventsFor(50);
    if (!writeCommands({QStringLiteral("buttonup right")}, error)) {
        return false;
    }
    processProbeEventsFor(200);
    if (!writeCommands({QStringLiteral("key down")}, error)) {
        return false;
    }
    processProbeEventsFor(80);
    if (!writeCommands({QStringLiteral("key enter")}, error)) {
        return false;
    }
    processProbeEventsFor(160);
    return true;
}

bool DotoolProcess::moveTo(const QPointF &point,
                           const QRectF &output,
                           QString *error)
{
    if (!output.contains(point)) {
        *error = QStringLiteral("dotool target lies outside the compositor output");
        return false;
    }
    const qreal x = std::clamp((point.x() - output.left()) / output.width(), 0.0, 1.0);
    const qreal y = std::clamp((point.y() - output.top()) / output.height(), 0.0, 1.0);
    return writeCommands(
        {QStringLiteral("mouseto %1 %2")
             .arg(QString::number(x, 'f', 8), QString::number(y, 'f', 8))}, error);
}

bool DotoolProcess::drag(const QPointF &start,
                         const QPointF &end,
                         const QRectF &output,
                         bool metaShift,
                         QString *error)
{
    if (!moveTo(start, output, error)) {
        return false;
    }
    processProbeEventsFor(120);
    if (metaShift) {
        if (!writeCommands({QStringLiteral("keydown super"),
                            QStringLiteral("keydown shift")}, error)) {
            return false;
        }
        processProbeEventsFor(80);
    }
    if (!writeCommands({QStringLiteral("buttondown left")}, error)) {
        return false;
    }
    processProbeEventsFor(60);

    constexpr int steps = 12;
    for (int step = 1; step <= steps; ++step) {
        const auto progress = qreal(step) / qreal(steps);
        const auto point = start + ((end - start) * progress);
        if (!moveTo(point, output, error)) {
            return false;
        }
        processProbeEventsFor(25);
    }
    if (!writeCommands({QStringLiteral("buttonup left")}, error)) {
        return false;
    }
    processProbeEventsFor(80);
    if (metaShift
        && !writeCommands({QStringLiteral("keyup shift"),
                           QStringLiteral("keyup super")}, error)) {
        return false;
    }
    processProbeEventsFor(120);
    return true;
}

bool DotoolProcess::isRunning() const
{
    return m_process.state() == QProcess::Running;
}

QString DotoolProcess::diagnostics() const
{
    const auto standardError = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
    const auto standardOutput = QString::fromUtf8(m_process.readAllStandardOutput()).trimmed();
    return QStringLiteral("state=%1 exitCode=%2 stderr='%3' stdout='%4'")
        .arg(int(m_process.state()))
        .arg(m_process.exitCode())
        .arg(standardError, standardOutput);
}

bool DotoolProcess::writeCommands(const QStringList &commands, QString *error)
{
    if (m_process.state() != QProcess::Running) {
        if (error) {
            *error = QStringLiteral("dotool exited before input injection; %1")
                         .arg(diagnostics());
        }
        return false;
    }
    QByteArray payload;
    for (const auto &command : commands) {
        payload.append(command.toUtf8());
        payload.append('\n');
    }
    if (m_process.write(payload) != payload.size()) {
        if (error) {
            *error = QStringLiteral("could not stream commands to dotool; %1")
                         .arg(diagnostics());
        }
        return false;
    }
    if (m_process.bytesToWrite() > 0 && !m_process.waitForBytesWritten(1000)) {
        if (error) {
            *error = QStringLiteral("dotool command stream stalled; %1")
                         .arg(diagnostics());
        }
        return false;
    }
    return true;
}

DevelopmentInputDriver::DevelopmentInputDriver(CompositorProbeClient &client)
    : m_client(client)
{
}

bool DevelopmentInputDriver::drag(const QPointF &start,
                                  const QPointF &end,
                                  bool metaShift,
                                  QString *error)
{
    if (!inject({pointerEvent(start)}, error)) {
        return false;
    }
    processProbeEventsFor(30);
    if (metaShift
        && !inject({keyEvent(QLatin1StringView("left-meta"), true),
                    keyEvent(QLatin1StringView("left-shift"), true)}, error)) {
        return false;
    }
    if (!inject({buttonEvent(QLatin1StringView("left"), true)}, error)) {
        return false;
    }

    constexpr int steps = 12;
    for (int step = 1; step <= steps; ++step) {
        const auto progress = qreal(step) / qreal(steps);
        if (!inject({pointerEvent(start + ((end - start) * progress))}, error)) {
            return false;
        }
        processProbeEventsFor(15);
    }
    if (!inject({buttonEvent(QLatin1StringView("left"), false)}, error)) {
        return false;
    }
    if (metaShift
        && !inject({keyEvent(QLatin1StringView("left-shift"), false),
                    keyEvent(QLatin1StringView("left-meta"), false)}, error)) {
        return false;
    }
    processProbeEventsFor(60);
    return true;
}

bool DevelopmentInputDriver::activateFirstContextMenuAction(
    const QPointF &point,
    QString *error)
{
    if (!inject({pointerEvent(point)}, error)) {
        return false;
    }
    processProbeEventsFor(30);
    if (!inject({buttonEvent(QLatin1StringView("right"), true)}, error)) {
        return false;
    }
    processProbeEventsFor(30);
    if (!inject({buttonEvent(QLatin1StringView("right"), false)}, error)) {
        return false;
    }
    processProbeEventsFor(200);
    if (!inject({keyEvent(QLatin1StringView("down"), true),
                 keyEvent(QLatin1StringView("down"), false)}, error)) {
        return false;
    }
    processProbeEventsFor(80);
    if (!inject({keyEvent(QLatin1StringView("enter"), true),
                 keyEvent(QLatin1StringView("enter"), false)}, error)) {
        return false;
    }
    processProbeEventsFor(160);
    return true;
}

bool DevelopmentInputDriver::inject(const QJsonArray &events, QString *error)
{
    const QJsonObject request{{QStringLiteral("schemaVersion"), 1},
                              {QStringLiteral("events"), events}};
    const auto reply = m_client.call(
        QStringLiteral("InjectTestInput"),
        QJsonDocument(request).toJson(QJsonDocument::Compact), error);
    if (!reply) {
        return false;
    }
    if (reply->value(QStringLiteral("status")) != QStringLiteral("injected")
        || reply->value(QStringLiteral("eventCount")).toInt(-1) != events.size()
        || reply->value(QStringLiteral("deviceId")).toString().isEmpty()) {
        *error = QStringLiteral("development input endpoint rejected a valid event batch: %1")
                     .arg(QString::fromUtf8(
                         QJsonDocument(*reply).toJson(QJsonDocument::Compact)));
        return false;
    }
    const auto replyDeviceId = reply->value(QStringLiteral("deviceId")).toString();
    if (!m_deviceId.isEmpty() && m_deviceId != replyDeviceId) {
        *error = QStringLiteral("development input device identity changed during one gesture");
        return false;
    }
    m_deviceId = replyDeviceId;
    ++m_requestCount;
    return true;
}

} // namespace QindaQt::Test
