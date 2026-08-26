// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QProcess>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace QindaQt::Test {

class CompositorProbeClient;

void processProbeEventsFor(int milliseconds);

class DotoolProcess final
{
public:
    DotoolProcess() = default;
    ~DotoolProcess();

    DotoolProcess(const DotoolProcess &) = delete;
    DotoolProcess &operator=(const DotoolProcess &) = delete;

    [[nodiscard]] bool start(const QString &program, QString *error);
    [[nodiscard]] bool moveTo(const QPointF &point,
                              const QRectF &output,
                              QString *error);
    [[nodiscard]] bool drag(const QPointF &start,
                            const QPointF &end,
                            const QRectF &output,
                            bool metaShift,
                            QString *error);
    [[nodiscard]] bool activateFirstContextMenuAction(
        const QPointF &point,
        const QRectF &output,
        QString *error);
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString diagnostics() const;

private:
    [[nodiscard]] bool writeCommands(const QStringList &commands, QString *error);

    mutable QProcess m_process;
};

// Development fallback for KWin's virtual backend, which deliberately has no
// libinput devices. Requests enter through the gated compositor test endpoint
// and are emitted by a KWin InputDevice, so the normal spy/filter chain is
// still exercised. This class never calls topology or controller APIs.
class DevelopmentInputDriver final
{
public:
    explicit DevelopmentInputDriver(CompositorProbeClient &client);

    [[nodiscard]] bool drag(const QPointF &start,
                            const QPointF &end,
                            bool metaShift,
                            QString *error);
    [[nodiscard]] bool activateFirstContextMenuAction(
        const QPointF &point,
        QString *error);
    [[nodiscard]] QString deviceId() const { return m_deviceId; }
    [[nodiscard]] int requestCount() const noexcept { return m_requestCount; }

private:
    [[nodiscard]] bool inject(const QJsonArray &events, QString *error);

    CompositorProbeClient &m_client;
    QString m_deviceId;
    int m_requestCount = 0;
};

} // namespace QindaQt::Test
