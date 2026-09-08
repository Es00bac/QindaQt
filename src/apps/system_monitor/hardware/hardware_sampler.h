#pragma once

#include <QVariantMap>

namespace QindaQt::SystemMonitor {

/**
 * Reads a bounded snapshot of Linux hardware telemetry from injected procfs
 * and sysfs roots.
 *
 * The sampler is reentrant and has no QObject or GUI affinity. The caller
 * owns the instance and must invoke sample() from the worker thread used for
 * telemetry collection. Missing or unsupported values are omitted from the
 * capability map and named in that GPU's `unavailable` map; a missing sensor
 * never becomes a fabricated zero.
 */
class HardwareSampler final
{
public:
    explicit HardwareSampler(QString sysRoot = QStringLiteral("/sys"),
                             QString procRoot = QStringLiteral("/proc"));

    [[nodiscard]] QVariantMap sample() const;

private:
    QString m_sysRoot;
    QString m_procRoot;
};

} // namespace QindaQt::SystemMonitor
