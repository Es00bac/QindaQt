// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_writer/output_management_port.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <algorithm>
#include <cmath>

namespace QindaQt::DisplayWriter
{
namespace
{

bool validMode(const ModeReference &mode)
{
    return mode.pixelSize.width() > 0 && mode.pixelSize.height() > 0
        && mode.pixelSize.width() <= Display::kMaxPixelDimension
        && mode.pixelSize.height() <= Display::kMaxPixelDimension
        && mode.refreshMilliHertz > 0
        && mode.refreshMilliHertz <= Display::kMaxRefreshMilliHertz;
}

bool emptyMode(const ModeReference &mode)
{
    return mode.pixelSize.isEmpty() && mode.refreshMilliHertz == 0;
}

bool hasReplicationCycle(const QString &connector,
                         const QHash<QString, QString> &sources,
                         QSet<QString> &visiting, QSet<QString> &visited)
{
    if (visiting.contains(connector)) {
        return true;
    }
    if (visited.contains(connector)) {
        return false;
    }
    visiting.insert(connector);
    const QString source = sources.value(connector);
    if (!source.isEmpty()
        && hasReplicationCycle(source, sources, visiting, visited)) {
        return true;
    }
    visiting.remove(connector);
    visited.insert(connector);
    return false;
}

} // namespace

bool validateConfiguration(const Configuration &configuration)
{
    if (configuration.requestId == 0 || configuration.outputs.isEmpty()
        || configuration.outputs.size() > Display::kMaxOutputs) {
        return false;
    }
    if (configuration.scope != ConfigurationScope::CompleteTopology
        && configuration.scope != ConfigurationScope::SurvivingProperties) {
        return false;
    }

    QSet<QString> connectors;
    QSet<quint32> priorities;
    QHash<QString, QString> replicationSources;
    qsizetype enabledCount = 0;
    qsizetype primaryCount = 0;
    for (const OutputChange &output : configuration.outputs) {
        if (output.connectorName.isEmpty()
            || !Display::isBoundedText(output.connectorName,
                                       Display::kMaxConnectorNameUtf8Bytes)
            || connectors.contains(output.connectorName)
            || (output.enabled ? !validMode(output.mode)
                               : (!emptyMode(output.mode) && !validMode(output.mode)))
            || !std::isfinite(output.scale)
            || output.scale < Display::kMinimumScale
            || output.scale > Display::kMaximumScale
            || static_cast<quint32>(output.transform)
                > static_cast<quint32>(Display::Transform::FlipX270)) {
            return false;
        }
        connectors.insert(output.connectorName);
        replicationSources.insert(output.connectorName,
                                  output.replicationSourceConnector);

        if (configuration.scope == ConfigurationScope::SurvivingProperties) {
            if (!output.enabled || output.primary || output.priority != 0
                || !output.position.isNull()
                || !output.replicationSourceConnector.isEmpty()) {
                return false;
            }
            continue;
        }
        if (!output.enabled) {
            if (output.primary || output.priority != 0 || !output.position.isNull()
                || !output.replicationSourceConnector.isEmpty()) {
                return false;
            }
            continue;
        }
        ++enabledCount;
        primaryCount += output.primary ? 1 : 0;
        if (output.priority == 0 || priorities.contains(output.priority)
            || std::abs(static_cast<qint64>(output.position.x()))
                > Display::kCoordinateBound
            || std::abs(static_cast<qint64>(output.position.y()))
                > Display::kCoordinateBound) {
            return false;
        }
        priorities.insert(output.priority);
    }

    if (configuration.scope == ConfigurationScope::SurvivingProperties) {
        return true;
    }
    if (enabledCount == 0 || primaryCount != 1) {
        return false;
    }
    for (quint32 priority = 1; priority <= static_cast<quint32>(enabledCount);
         ++priority) {
        if (!priorities.contains(priority)) {
            return false;
        }
    }
    for (const OutputChange &output : configuration.outputs) {
        if (output.replicationSourceConnector.isEmpty()) {
            continue;
        }
        const auto source = std::find_if(
            configuration.outputs.cbegin(), configuration.outputs.cend(),
            [&](const OutputChange &candidate) {
                return candidate.connectorName
                    == output.replicationSourceConnector;
            });
        if (!output.enabled || source == configuration.outputs.cend()
            || !source->enabled || source->connectorName == output.connectorName) {
            return false;
        }
    }
    QSet<QString> visiting;
    QSet<QString> visited;
    for (const OutputChange &output : configuration.outputs) {
        if (hasReplicationCycle(output.connectorName, replicationSources,
                                visiting, visited)) {
            return false;
        }
    }
    return true;
}

} // namespace QindaQt::DisplayWriter
