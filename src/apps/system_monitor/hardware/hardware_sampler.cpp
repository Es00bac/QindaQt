#include "hardware_sampler.h"
#include "nvidia_telemetry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <optional>
#include <utility>

namespace QindaQt::SystemMonitor {
namespace {

constexpr int maxTextBytes = 16 * 1024;
constexpr int maxDevices = 64;

QString rooted(const QString &root, const QString &relative)
{
    return QDir::cleanPath(QDir(root).filePath(relative));
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.read(maxTextBytes)).trimmed();
}

bool readInteger(const QString &path, qlonglong *value)
{
    bool ok = false;
    const qlonglong parsed = readText(path).toLongLong(&ok);
    if (!ok) {
        return false;
    }
    *value = parsed;
    return true;
}

void markUnavailable(QVariantMap *unavailable, const QString &name, const QString &reason)
{
    unavailable->insert(name, reason);
}

QString driverName(const QString &device)
{
    const QFileInfo link(device + QStringLiteral("/driver"));
    if (!link.exists()) {
        return {};
    }
    const QString target = link.symLinkTarget();
    return target.isEmpty() ? QString{} : QFileInfo(target).fileName();
}

QString sensorLabel(const QString &directory, const QString &prefix, int index,
                   const QString &fallback)
{
    const QString label = readText(directory + QLatin1Char('/') + prefix
                                    + QString::number(index) + QStringLiteral("_label"));
    return label.isEmpty() ? fallback : label;
}

QString ueventValue(const QString &path, const QString &key)
{
    for (const QString &line : readText(path).split(QLatin1Char('\n'))) {
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator > 0 && line.left(separator) == key) {
            return line.mid(separator + 1).trimmed();
        }
    }
    return {};
}

void addSensor(QVariantList *sensors, const QString &id, const QString &name,
               const QString &kind, double value, const QString &unit)
{
    sensors->append(QVariantMap{{QStringLiteral("id"), id},
                                {QStringLiteral("name"), name},
                                {QStringLiteral("kind"), kind},
                                {QStringLiteral("value"), value},
                                {QStringLiteral("unit"), unit}});
}

struct HwmonReading
{
    QString canonicalDevice;
    QString chipName;
    std::optional<double> temperature;
    std::optional<double> power;
};

struct HwmonSnapshot
{
    QVariantList sensors;
    QVector<HwmonReading> readings;
};

HwmonSnapshot collectHwmon(const QString &sysRoot)
{
    HwmonSnapshot snapshot;
    const QDir hwmonRoot(rooted(sysRoot, QStringLiteral("class/hwmon")));
    const QStringList directories = hwmonRoot.entryList(
        {QStringLiteral("hwmon[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &directoryName : directories.mid(0, maxDevices)) {
        const QString directory = hwmonRoot.filePath(directoryName);
        const QString chipName = readText(directory + QStringLiteral("/name"));
        HwmonReading reading{QFileInfo(rooted(directory, QStringLiteral("device")))
                                 .canonicalFilePath(),
                             chipName,
                             std::nullopt,
                             std::nullopt};
        const QString pciSlot = ueventValue(
            rooted(directory, QStringLiteral("device/uevent")), QStringLiteral("PCI_SLOT_NAME"));
        const QString sensorPrefix = !pciSlot.isEmpty()
            ? QStringLiteral("pci:") + pciSlot
            : (!reading.canonicalDevice.isEmpty()
                   ? QStringLiteral("device:") + reading.canonicalDevice
                   : (chipName.isEmpty() ? QStringLiteral("chip:unknown")
                                         : QStringLiteral("chip:") + chipName));
        const QStringList entries = QDir(directory).entryList(
            {QStringLiteral("temp[0-9]*_input"), QStringLiteral("power[0-9]*_input"),
             QStringLiteral("power[0-9]*_average"), QStringLiteral("fan[0-9]*_input"),
             QStringLiteral("in[0-9]*_input")}, QDir::Files, QDir::Name);
        for (const QString &entry : entries.mid(0, maxDevices)) {
            const QRegularExpression match(QStringLiteral("^([a-z]+)([0-9]+)_(input|average)$"));
            const QRegularExpressionMatch parts = match.match(entry);
            if (!parts.hasMatch()) {
                continue;
            }
            const QString kindPrefix = parts.captured(1);
            const int index = parts.captured(2).toInt();
            qlonglong raw = 0;
            if (!readInteger(directory + QLatin1Char('/') + entry, &raw)) {
                continue;
            }
            double value = 0.0;
            QString kind;
            QString unit;
            if (kindPrefix == QLatin1String("temp")) {
                value = static_cast<double>(raw) / 1000.0;
                kind = QStringLiteral("temperature");
                unit = QStringLiteral("C");
            } else if (kindPrefix == QLatin1String("power")) {
                value = static_cast<double>(raw) / 1000000.0;
                kind = QStringLiteral("power");
                unit = QStringLiteral("W");
            } else if (kindPrefix == QLatin1String("fan")) {
                value = static_cast<double>(raw);
                kind = QStringLiteral("fan");
                unit = QStringLiteral("RPM");
            } else if (kindPrefix == QLatin1String("in")) {
                value = static_cast<double>(raw) / 1000.0;
                kind = QStringLiteral("voltage");
                unit = QStringLiteral("V");
            } else {
                continue;
            }
            const QString fallback = chipName.isEmpty() ? kind : chipName;
            const QString label = sensorLabel(directory, kindPrefix, index, fallback);
            const QString id = sensorPrefix + QLatin1Char('/') + entry;
            addSensor(&snapshot.sensors, id, label, kind, value, unit);
            if (kind == QLatin1String("temperature") && !reading.temperature.has_value()) {
                reading.temperature = value;
            }
            if (kind == QLatin1String("power") && !reading.power.has_value()) {
                reading.power = value;
            }
        }
        snapshot.readings.append(reading);
    }
    return snapshot;
}

QString stableGpuId(const QString &device, const QString &card)
{
    const QString slot = ueventValue(device + QStringLiteral("/uevent"),
                                      QStringLiteral("PCI_SLOT_NAME"));
    if (!slot.isEmpty()) {
        return QStringLiteral("pci:") + slot;
    }
    const QString canonical = QFileInfo(device).canonicalFilePath();
    return canonical.isEmpty() ? QStringLiteral("drm:") + card
                               : QStringLiteral("device:") + canonical;
}


QVariantMap sampleCpu(const QString &sysRoot, const QString &procRoot)
{
    QVariantMap result;
    QVariantMap unavailable;
    const QString cpuInfo = readText(rooted(procRoot, QStringLiteral("cpuinfo")));
    const QStringList lines = cpuInfo.split(QLatin1Char('\n'));
    double procFrequency = 0.0;
    for (const QString &line : lines) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();
        if ((key == QLatin1String("model name") || key == QLatin1String("Hardware"))
            && !result.contains(QStringLiteral("cpuModel"))) {
            result.insert(QStringLiteral("cpuModel"), value);
        } else if (key == QLatin1String("cpu MHz") && procFrequency <= 0.0) {
            procFrequency = value.toDouble();
        }
    }
    if (!result.contains(QStringLiteral("cpuModel"))) {
        markUnavailable(&unavailable, QStringLiteral("cpuModel"),
                        QStringLiteral("proc-cpuinfo-model-missing"));
    }

    qlonglong frequencyKHz = 0;
    const QStringList frequencyPaths{
        rooted(sysRoot, QStringLiteral("devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")),
        rooted(sysRoot, QStringLiteral("devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq"))};
    for (const QString &path : frequencyPaths) {
        if (readInteger(path, &frequencyKHz) && frequencyKHz > 0) {
            result.insert(QStringLiteral("frequencyMHz"),
                          static_cast<double>(frequencyKHz) / 1000.0);
            break;
        }
    }
    if (!result.contains(QStringLiteral("frequencyMHz")) && procFrequency > 0.0) {
        result.insert(QStringLiteral("frequencyMHz"), procFrequency);
    }
    if (!result.contains(QStringLiteral("frequencyMHz"))) {
        markUnavailable(&unavailable, QStringLiteral("frequencyMHz"),
                        QStringLiteral("cpu-frequency-unavailable"));
    }
    if (!unavailable.isEmpty()) {
        result.insert(QStringLiteral("unavailable"), unavailable);
    }
    return result;
}

} // namespace

HardwareSampler::HardwareSampler(QString sysRoot, QString procRoot)
    : m_sysRoot(QDir::cleanPath(std::move(sysRoot)))
    , m_procRoot(QDir::cleanPath(std::move(procRoot)))
{
}

QVariantMap HardwareSampler::sample() const
{
    QVariantMap result = sampleCpu(m_sysRoot, m_procRoot);
    QVariantList gpus;
    const HwmonSnapshot hwmon = collectHwmon(m_sysRoot);
    const QDir drmRoot(rooted(m_sysRoot, QStringLiteral("class/drm")));
    // AGENT-GUARD: DRM connector directories (card0-HDMI-A-1, etc.) are
    // surfaces, not GPUs; admitting them duplicates telemetry and breaks IDs.
    const QStringList entries = drmRoot.entryList(
        {QStringLiteral("card*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    const QRegularExpression cardPattern(QStringLiteral("^card[0-9]+$"));
    QStringList cardNames;
    for (const QString &entry : entries) {
        if (cardPattern.match(entry).hasMatch()) {
            cardNames.append(entry);
        }
    }
    int amdCards = 0;
    for (const QString &card : cardNames) {
        const QString device = drmRoot.filePath(card) + QStringLiteral("/device");
        const QString vendor = readText(device + QStringLiteral("/vendor")).toLower();
        if (vendor == QLatin1String("0x1002")) {
            ++amdCards;
        }
    }
    int cardCount = 0;
    for (const QString &card : cardNames) {
        if (cardCount++ >= maxDevices) {
            break;
        }
        // AGENT-CONTRACT: PCI or canonical-device identity is stable across
        // DRM connector order; `drm:cardN` is only the final fixture fallback.
        const QString cardPath = drmRoot.filePath(card);
        const QString device = cardPath + QStringLiteral("/device");
        const QString vendor = readText(device + QStringLiteral("/vendor")).toLower();
        const QString gpuId = stableGpuId(device, card);
        const QString driver = driverName(device);
        QVariantMap gpu{{QStringLiteral("id"), gpuId},
                        {QStringLiteral("name"),
                         readText(device + QStringLiteral("/product_name"))}};
        QVariantMap unavailable;
        if (gpu.value(QStringLiteral("name")).toString().isEmpty()) {
            gpu.insert(QStringLiteral("name"), vendor == QLatin1String("0x1002")
                                                 ? QStringLiteral("AMD GPU")
                                                 : gpuId);
        }
        qlonglong value = 0;
        if (readInteger(device + QStringLiteral("/gpu_busy_percent"), &value)
            && value >= 0 && value <= 100) {
            gpu.insert(QStringLiteral("busy"), value);
        } else {
            markUnavailable(&unavailable, QStringLiteral("busy"),
                            QStringLiteral("gpu-busy-percent-unavailable"));
        }
        if (readInteger(device + QStringLiteral("/mem_info_vram_used"), &value) && value >= 0) {
            gpu.insert(QStringLiteral("memoryUsed"), value);
        } else {
            markUnavailable(&unavailable, QStringLiteral("memoryUsed"),
                            QStringLiteral("vram-used-unavailable"));
        }
        if (readInteger(device + QStringLiteral("/mem_info_vram_total"), &value) && value > 0) {
            gpu.insert(QStringLiteral("memoryTotal"), value);
        } else {
            markUnavailable(&unavailable, QStringLiteral("memoryTotal"),
                            QStringLiteral("vram-total-unavailable"));
        }
        const QString canonicalDevice = QFileInfo(device).canonicalFilePath();
        const bool allowChipFallback = amdCards == 1 && vendor == QLatin1String("0x1002");
        for (const HwmonReading &reading : hwmon.readings) {
            const bool sameDevice = !canonicalDevice.isEmpty()
                && reading.canonicalDevice == canonicalDevice;
            const bool sameChip = allowChipFallback
                && reading.chipName.compare(QStringLiteral("amdgpu"), Qt::CaseInsensitive) == 0;
            if (!sameDevice && !sameChip) {
                continue;
            }
            if (reading.temperature.has_value()) {
                gpu.insert(QStringLiteral("temperature"), *reading.temperature);
            }
            if (reading.power.has_value()) {
                gpu.insert(QStringLiteral("power"), *reading.power);
            }
            break;
        }
        if (!gpu.contains(QStringLiteral("temperature"))) {
            markUnavailable(&unavailable, QStringLiteral("temperature"),
                            QStringLiteral("gpu-temperature-unavailable"));
        }
        if (!gpu.contains(QStringLiteral("power"))) {
            markUnavailable(&unavailable, QStringLiteral("power"),
                            QStringLiteral("gpu-power-unavailable"));
        }
        if (vendor == QLatin1String("0x10de")) {
            const QString pciBusId = ueventValue(device + QStringLiteral("/uevent"),
                                                  QStringLiteral("PCI_SLOT_NAME"));
            const Hardware::NvidiaTelemetry nvidia
                = Hardware::NvidiaTelemetryProvider::sample(pciBusId);
            if (nvidia.values.isEmpty()) {
                markUnavailable(&unavailable, QStringLiteral("nvidia"), nvidia.unavailableReason);
            } else {
                for (auto it = nvidia.values.cbegin(); it != nvidia.values.cend(); ++it) {
                    unavailable.remove(it.key());
                    gpu.insert(it.key(), it.value());
                }
            }
        }
        // AGENT-CONTRACT: Engine accounting is omitted until a process/fd
        // ownership context is supplied; consumers must honor this reason.
        markUnavailable(&unavailable, QStringLiteral("engines"),
                        QStringLiteral("engine-accounting-requires-device-context"));
        if (!driver.isEmpty()) {
            gpu.insert(QStringLiteral("driver"), driver);
        } else {
            markUnavailable(&unavailable, QStringLiteral("driver"),
                            QStringLiteral("driver-link-unavailable"));
        }
        gpu.insert(QStringLiteral("unavailable"), unavailable);
        gpus.append(gpu);
    }
    result.insert(QStringLiteral("gpus"), gpus);
    result.insert(QStringLiteral("sensors"), hwmon.sensors);
    return result;
}

} // namespace QindaQt::SystemMonitor
