// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Test {

class CompositorProbeClient;

struct HybridDiagnostics final
{
    quint64 revision = 0;
    int containerCount = 0;
    QJsonObject json;
};

struct PublicContainerEvidence final
{
    QString containerId;
    QString revision;
    QJsonObject snapshot;
};

[[nodiscard]] QJsonObject rectJson(const QRectF &frame);
[[nodiscard]] QJsonObject pointJson(const QPointF &point);
[[nodiscard]] std::optional<QRectF>
singleOutputFrame(CompositorProbeClient &client, QString *error);
[[nodiscard]] std::optional<HybridDiagnostics>
readHybridDiagnostics(CompositorProbeClient &client, QString *error);
[[nodiscard]] std::optional<HybridDiagnostics> awaitHybridDiagnostics(
    CompositorProbeClient &client,
    const std::function<bool(const HybridDiagnostics &)> &ready,
    QString *error);
[[nodiscard]] std::optional<QJsonArray>
awaitDotoolDevices(CompositorProbeClient &client, QString *error);
[[nodiscard]] std::optional<PublicContainerEvidence> readPublicHybridContainer(
    CompositorProbeClient &client,
    const QString &expectedContainerId,
    quint64 expectedRevision,
    QString *error);

} // namespace QindaQt::Test
