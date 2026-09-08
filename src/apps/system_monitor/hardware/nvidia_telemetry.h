#pragma once

#include <QString>
#include <QVariantMap>

namespace QindaQt::SystemMonitor::Hardware {

struct NvidiaTelemetry
{
    QVariantMap values;
    QString unavailableReason;
};

/**
 * Optional runtime NVML bridge. It owns no library state between calls and
 * never makes NVML a link or package dependency of the hardware sampler.
 */
class NvidiaTelemetryProvider final
{
public:
    [[nodiscard]] static NvidiaTelemetry sample(const QString &pciBusId);
};

} // namespace QindaQt::SystemMonitor::Hardware
