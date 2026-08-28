// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_writer/output_management_port.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace QindaQt::DisplayWriter
{
namespace
{

MapResult rejected(MapError error, QString reason)
{
    return {.configuration = {}, .error = error, .reasonCode = std::move(reason)};
}

std::optional<QString> connectorForStableId(const QString &stableId)
{
    constexpr QLatin1StringView prefix("conn:");
    if (!stableId.startsWith(prefix)) {
        return std::nullopt;
    }
    const QString connector = stableId.sliced(prefix.size());
    if (connector.isEmpty()
        || !Display::isBoundedText(connector, Display::kMaxConnectorNameUtf8Bytes)) {
        return std::nullopt;
    }
    return connector;
}

std::optional<ModeReference> parseCurrentMode(const QString &modeId)
{
    static const QRegularExpression pattern(
        QStringLiteral("^current:([1-9][0-9]{0,4})x([1-9][0-9]{0,4})@([1-9][0-9]{0,6})$"),
        QRegularExpression::NoPatternOption);
    const QRegularExpressionMatch match = pattern.match(modeId);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool widthOk = false;
    bool heightOk = false;
    bool refreshOk = false;
    const int width = match.capturedView(1).toInt(&widthOk);
    const int height = match.capturedView(2).toInt(&heightOk);
    const quint32 refresh = match.capturedView(3).toUInt(&refreshOk);
    if (!widthOk || !heightOk || !refreshOk || width > Display::kMaxPixelDimension
        || height > Display::kMaxPixelDimension
        || refresh > Display::kMaxRefreshMilliHertz) {
        return std::nullopt;
    }
    return ModeReference{.pixelSize = QSize(width, height),
                         .refreshMilliHertz = refresh};
}

bool hasMirrorCycle(const QString &connector,
                    const QHash<QString, QString> &mirrorSources,
                    QSet<QString> &visiting, QSet<QString> &visited)
{
    if (visiting.contains(connector)) {
        return true;
    }
    if (visited.contains(connector)) {
        return false;
    }
    visiting.insert(connector);
    const QString source = mirrorSources.value(connector);
    if (!source.isEmpty()
        && hasMirrorCycle(source, mirrorSources, visiting, visited)) {
        return true;
    }
    visiting.remove(connector);
    visited.insert(connector);
    return false;
}

MapResult mapComplete(const DisplayTransaction::ApplyRequest &request,
                      const quint64 requestId)
{
    if (!Display::validateCandidate(request.candidate).accepted
        || !request.survivingProperties.isEmpty()) {
        return rejected(MapError::InvalidRequest, QStringLiteral("invalid-complete-request"));
    }

    Configuration configuration{.requestId = requestId,
                                .scope = ConfigurationScope::CompleteTopology,
                                .outputs = {}};
    configuration.outputs.reserve(request.candidate.outputs.size());
    QSet<QString> connectors;
    QSet<quint32> priorities;
    QHash<QString, QString> mirrorSources;
    qsizetype primaryCount = 0;
    qsizetype enabledCount = 0;

    for (const Display::CandidateOutput &output : request.candidate.outputs) {
        const auto connector = connectorForStableId(output.stableId);
        if (!connector || connectors.contains(*connector)) {
            return rejected(MapError::UnsupportedIdentity,
                            QStringLiteral("unsupported-or-duplicate-connector-identity"));
        }
        connectors.insert(*connector);

        OutputChange change{.connectorName = *connector,
                            .enabled = output.enabled,
                            .primary = output.primary,
                            .mode = {},
                            .position = output.position,
                            .scale = output.scale,
                            .transform = output.transform,
                            .priority = output.priority,
                            .replicationSourceConnector = {}};
        if (!output.modeId.isEmpty()) {
            const auto mode = parseCurrentMode(output.modeId);
            if (!mode) {
                return rejected(MapError::UnsupportedMode,
                                QStringLiteral("unsupported-current-mode-identity"));
            }
            change.mode = *mode;
        }
        if (output.enabled) {
            if (output.modeId.isEmpty()) {
                return rejected(MapError::UnsupportedMode,
                                QStringLiteral("unsupported-current-mode-identity"));
            }
            ++enabledCount;
            primaryCount += output.primary ? 1 : 0;
            if (output.priority == 0 || priorities.contains(output.priority)) {
                return rejected(MapError::InvalidTopology,
                                QStringLiteral("invalid-output-priority"));
            }
            priorities.insert(output.priority);
            if (!output.replicationSourceStableId.isEmpty()) {
                const auto source = connectorForStableId(
                    output.replicationSourceStableId);
                if (!source) {
                    return rejected(MapError::UnsupportedIdentity,
                                    QStringLiteral("unsupported-replication-identity"));
                }
                change.replicationSourceConnector = *source;
            }
        } else if (output.primary || output.priority != 0 || !output.position.isNull()
                   || !output.replicationSourceStableId.isEmpty()) {
            return rejected(MapError::InvalidTopology,
                            QStringLiteral("noncanonical-disabled-output"));
        }
        mirrorSources.insert(change.connectorName,
                             change.replicationSourceConnector);
        configuration.outputs.push_back(std::move(change));
    }

    if (enabledCount == 0 || primaryCount != 1) {
        return rejected(MapError::InvalidTopology,
                        QStringLiteral("invalid-primary-or-enabled-count"));
    }
    for (quint32 priority = 1; priority <= static_cast<quint32>(enabledCount);
         ++priority) {
        if (!priorities.contains(priority)) {
            return rejected(MapError::InvalidTopology,
                            QStringLiteral("noncontiguous-priority"));
        }
    }
    for (const OutputChange &output : std::as_const(configuration.outputs)) {
        if (output.replicationSourceConnector.isEmpty()) {
            continue;
        }
        const auto source = std::find_if(
            configuration.outputs.cbegin(), configuration.outputs.cend(),
            [&](const OutputChange &candidate) {
                return candidate.connectorName == output.replicationSourceConnector;
            });
        if (source == configuration.outputs.cend() || !source->enabled
            || source->connectorName == output.connectorName) {
            return rejected(MapError::InvalidTopology,
                            QStringLiteral("invalid-replication-source"));
        }
    }
    QSet<QString> visiting;
    QSet<QString> visited;
    for (const OutputChange &output : std::as_const(configuration.outputs)) {
        if (output.enabled
            && hasMirrorCycle(output.connectorName, mirrorSources, visiting, visited)) {
            return rejected(MapError::InvalidTopology,
                            QStringLiteral("replication-cycle"));
        }
    }
    if (!validateConfiguration(configuration)) {
        return rejected(MapError::InvalidTopology,
                        QStringLiteral("invalid-mapped-configuration"));
    }
    return {.configuration = std::move(configuration),
            .error = MapError::None,
            .reasonCode = {}};
}

MapResult mapSurvivors(const DisplayTransaction::ApplyRequest &request,
                       const quint64 requestId)
{
    if (!request.candidate.outputs.isEmpty() || request.survivingProperties.isEmpty()
        || request.survivingProperties.size() > Display::kMaxOutputs) {
        return rejected(MapError::InvalidRequest,
                        QStringLiteral("invalid-surviving-request"));
    }
    Configuration configuration{.requestId = requestId,
                                .scope = ConfigurationScope::SurvivingProperties,
                                .outputs = {}};
    configuration.outputs.reserve(request.survivingProperties.size());
    QSet<QString> connectors;
    for (const DisplayTransaction::SurvivingOutputProperties &output :
         request.survivingProperties) {
        const auto connector = connectorForStableId(output.stableId);
        const auto mode = parseCurrentMode(output.modeId);
        if (!connector || connectors.contains(*connector)) {
            return rejected(MapError::UnsupportedIdentity,
                            QStringLiteral("unsupported-or-duplicate-survivor-identity"));
        }
        if (!mode || !std::isfinite(output.scale)
            || output.scale < Display::kMinimumScale
            || output.scale > Display::kMaximumScale
            || static_cast<quint32>(output.transform)
                > static_cast<quint32>(Display::Transform::FlipX270)) {
            return rejected(MapError::UnsupportedMode,
                            QStringLiteral("invalid-surviving-properties"));
        }
        connectors.insert(*connector);
        configuration.outputs.push_back(
            {.connectorName = *connector,
             .enabled = true,
             .primary = false,
             .mode = *mode,
             .position = {},
             .scale = output.scale,
             .transform = output.transform,
             .priority = 0,
             .replicationSourceConnector = {}});
    }
    if (!validateConfiguration(configuration)) {
        return rejected(MapError::InvalidTopology,
                        QStringLiteral("invalid-mapped-survivors"));
    }
    return {.configuration = std::move(configuration),
            .error = MapError::None,
            .reasonCode = {}};
}

} // namespace

MapResult mapApplyRequest(const DisplayTransaction::ApplyRequest &request,
                          const quint64 requestId)
{
    if (requestId == 0 || request.token == 0) {
        return rejected(MapError::InvalidRequest, QStringLiteral("invalid-request-id"));
    }
    switch (request.scope) {
    case DisplayTransaction::ApplyScope::ForwardCandidate:
    case DisplayTransaction::ApplyScope::FullPreimage:
        return mapComplete(request, requestId);
    case DisplayTransaction::ApplyScope::SurvivingOutputProperties:
        return mapSurvivors(request, requestId);
    }
    return rejected(MapError::InvalidRequest, QStringLiteral("unknown-apply-scope"));
}

} // namespace QindaQt::DisplayWriter
