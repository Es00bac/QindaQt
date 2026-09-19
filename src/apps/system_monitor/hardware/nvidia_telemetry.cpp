#include "nvidia_telemetry.h"

#include <QLibrary>

namespace QindaQt::SystemMonitor::Hardware {
namespace {

struct NvmlUtilization
{
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory
{
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

using NvmlDevice = void *;
using NvmlReturn = unsigned int;
using NvmlInit = NvmlReturn (*)();
using NvmlShutdown = NvmlReturn (*)();
using NvmlGetHandle = NvmlReturn (*)(const char *, NvmlDevice *);
using NvmlGetName = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
using NvmlGetUtilization = NvmlReturn (*)(NvmlDevice, NvmlUtilization *);
using NvmlGetMemory = NvmlReturn (*)(NvmlDevice, NvmlMemory *);
using NvmlGetTemperature = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);
using NvmlGetPower = NvmlReturn (*)(NvmlDevice, unsigned int *);

} // namespace

NvidiaTelemetry NvidiaTelemetryProvider::sample(const QString &pciBusId)
{
    if (pciBusId.isEmpty()) {
        return {{}, QStringLiteral("nvidia-pci-bus-id-unavailable")};
    }
    QLibrary library(QStringLiteral("libnvidia-ml.so.1"));
    if (!library.load()) {
        return {{}, QStringLiteral("nvidia-runtime-unavailable")};
    }
    const auto init = reinterpret_cast<NvmlInit>(library.resolve("nvmlInit_v2"));
    const auto shutdown = reinterpret_cast<NvmlShutdown>(library.resolve("nvmlShutdown"));
    const auto getHandleV2 = reinterpret_cast<NvmlGetHandle>(
        library.resolve("nvmlDeviceGetHandleByPciBusId_v2"));
    const auto getHandleLegacy = reinterpret_cast<NvmlGetHandle>(
        library.resolve("nvmlDeviceGetHandleByPciBusId"));
    const auto getName = reinterpret_cast<NvmlGetName>(library.resolve("nvmlDeviceGetName"));
    const auto getUtilization = reinterpret_cast<NvmlGetUtilization>(
        library.resolve("nvmlDeviceGetUtilizationRates"));
    const auto getMemory = reinterpret_cast<NvmlGetMemory>(library.resolve("nvmlDeviceGetMemoryInfo"));
    const auto getTemperature = reinterpret_cast<NvmlGetTemperature>(
        library.resolve("nvmlDeviceGetTemperature"));
    const auto getPower = reinterpret_cast<NvmlGetPower>(library.resolve("nvmlDeviceGetPowerUsage"));
    if (!init || !shutdown || (!getHandleV2 && !getHandleLegacy) || !getName || !getUtilization
        || !getMemory || !getTemperature || !getPower) {
        return {{}, QStringLiteral("nvidia-runtime-api-incomplete")};
    }
    if (init() != 0) {
        return {{}, QStringLiteral("nvidia-runtime-init-failed")};
    }
    NvmlDevice device = nullptr;
    const QByteArray busId = pciBusId.toUtf8();
    const auto getHandle = getHandleV2 ? getHandleV2 : getHandleLegacy;
    if (getHandle(busId.constData(), &device) != 0 || !device) {
        shutdown();
        return {{}, QStringLiteral("nvidia-runtime-device-unavailable")};
    }

    QVariantMap values;
    char name[96] = {};
    NvmlUtilization utilization{};
    NvmlMemory memory{};
    unsigned int temperature = 0;
    unsigned int powerMilliwatts = 0;
    if (getName(device, name, sizeof(name)) == 0 && name[0] != '\0') {
        values.insert(QStringLiteral("name"), QString::fromUtf8(name));
    }
    if (getUtilization(device, &utilization) == 0 && utilization.gpu <= 100) {
        values.insert(QStringLiteral("busy"), utilization.gpu);
    }
    if (getMemory(device, &memory) == 0 && memory.total > 0) {
        values.insert(QStringLiteral("memoryUsed"), QVariant::fromValue(memory.used));
        values.insert(QStringLiteral("memoryTotal"), QVariant::fromValue(memory.total));
    }
    if (getTemperature(device, 0, &temperature) == 0) {
        values.insert(QStringLiteral("temperature"), temperature);
    }
    if (getPower(device, &powerMilliwatts) == 0) {
        values.insert(QStringLiteral("power"), static_cast<double>(powerMilliwatts) / 1000.0);
    }
    shutdown();
    return values.isEmpty()
        ? NvidiaTelemetry{{}, QStringLiteral("nvidia-runtime-no-readable-values")}
        : NvidiaTelemetry{values, {}};
}

} // namespace QindaQt::SystemMonitor::Hardware
