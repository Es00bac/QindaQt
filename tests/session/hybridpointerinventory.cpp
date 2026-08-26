// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointerinventory.h"

#include "compositorprobeclient.h"
#include "hybridtestinputdriver.h"

#include <QElapsedTimer>
#include <QJsonDocument>

#include <algorithm>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 4000;

std::optional<QRectF> parseRect(const QJsonValue &value, QString *error)
{
    if (!value.isObject()) {
        *error = QStringLiteral("output inventory returned a non-object geometry");
        return std::nullopt;
    }
    const auto object = value.toObject();
    for (const auto &field : {QStringLiteral("x"), QStringLiteral("y"),
                              QStringLiteral("width"), QStringLiteral("height")}) {
        if (!object.value(field).isDouble()) {
            *error = QStringLiteral("output inventory omitted numeric geometry field '%1'")
                         .arg(field);
            return std::nullopt;
        }
    }
    const QRectF frame(object.value(QStringLiteral("x")).toDouble(),
                       object.value(QStringLiteral("y")).toDouble(),
                       object.value(QStringLiteral("width")).toDouble(),
                       object.value(QStringLiteral("height")).toDouble());
    if (!frame.isValid()) {
        *error = QStringLiteral("output inventory returned invalid geometry");
        return std::nullopt;
    }
    return frame;
}

bool hasCapability(const QJsonObject &device, QLatin1StringView capability)
{
    const auto capabilities = device.value(QStringLiteral("capabilities")).toArray();
    return std::any_of(capabilities.cbegin(), capabilities.cend(),
                       [capability](const QJsonValue &value) {
                           return value.toString() == capability;
                       });
}

} // namespace

QJsonObject rectJson(const QRectF &frame)
{
    return {{QStringLiteral("x"), frame.x()},
            {QStringLiteral("y"), frame.y()},
            {QStringLiteral("width"), frame.width()},
            {QStringLiteral("height"), frame.height()}};
}

QJsonObject pointJson(const QPointF &point)
{
    return {{QStringLiteral("x"), point.x()},
            {QStringLiteral("y"), point.y()}};
}

std::optional<QRectF> singleOutputFrame(CompositorProbeClient &client, QString *error)
{
    const auto outputs = client.outputs(error);
    if (!outputs) {
        return std::nullopt;
    }
    // AGENT-CONTRACT: dotool's absolute uinput coordinates span one rectangle.
    // This first live gesture proof is intentionally registered only for the
    // single-1080p scenario; multi-output coordinate mapping needs its own
    // topology-aware injector before it can claim honest coverage.
    if (outputs->size() != 1 || !outputs->at(0).isObject()) {
        *error = QStringLiteral("Hybrid pointer proof requires exactly one compositor output");
        return std::nullopt;
    }
    return parseRect(outputs->at(0).toObject().value(QStringLiteral("geometry")), error);
}

std::optional<HybridDiagnostics> readHybridDiagnostics(CompositorProbeClient &client,
                                                        QString *error)
{
    const auto capabilities = client.call(QStringLiteral("Capabilities"), error);
    if (!capabilities) {
        return std::nullopt;
    }
    const auto hybridValue = capabilities->value(QStringLiteral("hybrid"));
    if (!hybridValue.isObject()) {
        *error = QStringLiteral("Capabilities omitted Hybrid diagnostics");
        return std::nullopt;
    }
    const auto hybrid = hybridValue.toObject();
    bool revisionValid = false;
    const auto revision = hybrid.value(QStringLiteral("topologyRevision"))
                              .toString()
                              .toULongLong(&revisionValid);
    const int containerCount = hybrid.value(QStringLiteral("containerCount")).toInt(-1);
    const int chromeOverlayCount =
        hybrid.value(QStringLiteral("chromeOverlayCount")).toInt(-1);
    const int visibleChromeOverlayCount =
        hybrid.value(QStringLiteral("visibleChromeOverlayCount")).toInt(-1);
    const int anchoredChromeSceneItemCount =
        hybrid.value(QStringLiteral("anchoredChromeSceneItemCount")).toInt(-1);
    const int visibleAnchoredChromeSceneItemCount = hybrid
        .value(QStringLiteral("visibleAnchoredChromeSceneItemCount"))
        .toInt(-1);
    const int quarantinedContainerCount =
        hybrid.value(QStringLiteral("quarantinedContainerCount")).toInt(-1);
    const int publishedGroupStackingCount =
        hybrid.value(QStringLiteral("publishedGroupStackingCount")).toInt(-1);
    if (!hybrid.value(QStringLiteral("ready")).toBool()
        || !hybrid.value(QStringLiteral("inputFilterInstalled")).toBool()
        || !revisionValid || containerCount < 0 || chromeOverlayCount < 0
        || visibleChromeOverlayCount < 0
        || visibleChromeOverlayCount > chromeOverlayCount
        || anchoredChromeSceneItemCount < 0
        || anchoredChromeSceneItemCount > chromeOverlayCount
        || visibleAnchoredChromeSceneItemCount < 0
        || visibleAnchoredChromeSceneItemCount > visibleChromeOverlayCount
        || visibleAnchoredChromeSceneItemCount > anchoredChromeSceneItemCount
        || quarantinedContainerCount < 0
        || quarantinedContainerCount > containerCount
        || publishedGroupStackingCount < 0
        || publishedGroupStackingCount > containerCount
        || !hybrid.value(QStringLiteral("lastGroupStackingFailure")).isString()) {
        *error = QStringLiteral("Capabilities returned invalid or inactive Hybrid diagnostics");
        return std::nullopt;
    }
    return HybridDiagnostics{revision, containerCount, hybrid};
}

std::optional<HybridDiagnostics> awaitHybridDiagnostics(
    CompositorProbeClient &client,
    const std::function<bool(const HybridDiagnostics &)> &ready,
    QString *error)
{
    QElapsedTimer timer;
    timer.start();
    QString lastError;
    QJsonObject lastDiagnostics;
    while (timer.elapsed() < InventoryTimeoutMilliseconds) {
        QString diagnosticsError;
        auto diagnostics = readHybridDiagnostics(client, &diagnosticsError);
        if (diagnostics) {
            lastDiagnostics = diagnostics->json;
            if (ready(*diagnostics)) {
                error->clear();
                return diagnostics;
            }
        } else if (!diagnosticsError.isEmpty()) {
            lastError = std::move(diagnosticsError);
        }
        processProbeEventsFor(20);
    }
    *error = lastError.isEmpty()
        ? QStringLiteral("timed out waiting for Hybrid diagnostics; last=%1")
              .arg(QString::fromUtf8(
                  QJsonDocument(lastDiagnostics).toJson(QJsonDocument::Compact)))
        : lastError;
    return std::nullopt;
}

std::optional<QJsonArray> awaitDotoolDevices(CompositorProbeClient &client, QString *error)
{
    QElapsedTimer timer;
    timer.start();
    QJsonArray lastDevices;
    QString lastError;
    while (timer.elapsed() < InventoryTimeoutMilliseconds) {
        QString inputError;
        const auto input = client.call(QStringLiteral("InputCapabilities"), &inputError);
        if (input && input->value(QStringLiteral("devices")).isArray()) {
            lastDevices = input->value(QStringLiteral("devices")).toArray();
            QJsonArray dotoolDevices;
            bool keyboard = false;
            bool pointer = false;
            for (const auto &value : std::as_const(lastDevices)) {
                const auto device = value.toObject();
                if (!device.value(QStringLiteral("name"))
                         .toString()
                         .contains(QStringLiteral("dotool"), Qt::CaseInsensitive)) {
                    continue;
                }
                dotoolDevices.append(device);
                keyboard = keyboard || hasCapability(device, QLatin1StringView("keyboard"));
                pointer = pointer || hasCapability(device, QLatin1StringView("pointer"));
            }
            if (keyboard && pointer) {
                error->clear();
                return dotoolDevices;
            }
        } else if (!inputError.isEmpty()) {
            lastError = std::move(inputError);
        }
        processProbeEventsFor(20);
    }
    *error = lastError.isEmpty()
        ? QStringLiteral("KWin's virtual backend did not publish dotool's uinput keyboard "
                         "and pointer; devices=%1")
              .arg(QString::fromUtf8(
                  QJsonDocument(lastDevices).toJson(QJsonDocument::Compact)))
        : lastError;
    return std::nullopt;
}

std::optional<PublicContainerEvidence> readPublicHybridContainer(
    CompositorProbeClient &client,
    const QString &expectedContainerId,
    quint64 expectedRevision,
    QString *error)
{
    const auto containers = client.containers(error);
    if (!containers) {
        return std::nullopt;
    }
    if (containers->size() != 1 || !containers->at(0).isObject()) {
        *error = QStringLiteral("Containers did not expose exactly one Hybrid owner");
        return std::nullopt;
    }
    const auto container = containers->at(0).toObject();
    const auto containerId = container.value(QStringLiteral("id")).toString();
    const auto revisionText = container.value(QStringLiteral("revision")).toString();
    bool revisionValid = false;
    const auto revision = revisionText.toULongLong(&revisionValid);
    if (containerId != expectedContainerId
        || container.value(QStringLiteral("authority"))
               != QStringLiteral("hybrid-process")
        || !revisionValid || revision == 0 || revision != expectedRevision) {
        *error = QStringLiteral("Containers published stale or non-Hybrid state: %1")
                     .arg(QString::fromUtf8(
                         QJsonDocument(*containers).toJson(QJsonDocument::Compact)));
        return std::nullopt;
    }
    const auto snapshot = client.call(QStringLiteral("Snapshot"), containerId, error);
    if (!snapshot) {
        return std::nullopt;
    }
    const auto model = snapshot->value(QStringLiteral("snapshot")).toObject();
    if (snapshot->value(QStringLiteral("status")) != QStringLiteral("ok")
        || snapshot->value(QStringLiteral("authority"))
               != QStringLiteral("hybrid-process")
        || snapshot->value(QStringLiteral("containerId")).toString() != containerId
        || snapshot->value(QStringLiteral("revision")).toString() != revisionText
        || model.value(QStringLiteral("id")).toString() != containerId
        || model.value(QStringLiteral("pages")).toArray().isEmpty()) {
        *error = QStringLiteral("Snapshot did not expose the grouped Hybrid model: %1")
                     .arg(QString::fromUtf8(
                         QJsonDocument(*snapshot).toJson(QJsonDocument::Compact)));
        return std::nullopt;
    }
    return PublicContainerEvidence{containerId, revisionText, *snapshot};
}

} // namespace QindaQt::Test
