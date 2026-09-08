#include "hardware/hardware_sampler.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using QindaQt::SystemMonitor::HardwareSampler;

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    QCOMPARE(file.write(contents), contents.size());
}

QVariantMap firstGpu(const QVariantMap &snapshot)
{
    const QVariantList gpus = snapshot.value(QStringLiteral("gpus")).toList();
    return gpus.isEmpty() ? QVariantMap{} : gpus.first().toMap();
}

QVariantMap sensorById(const QVariantList &sensors, const QString &id)
{
    for (const QVariant &value : sensors) {
        const QVariantMap sensor = value.toMap();
        if (sensor.value(QStringLiteral("id")) == id) {
            return sensor;
        }
    }
    return {};
}

} // namespace

class HardwareSamplerTest final : public QObject
{
    Q_OBJECT

private slots:
    void amdSnapshotReadsRealCapabilities();
    void malformedAndMissingValuesAreUnavailable();
    void unsupportedGpuDoesNotClaimEngineOrTelemetryValues();
    void cpuAndSensorsRemainAvailableWithoutDrm();
    void multipleGpusKeepStableIdsAndUniqueSensors();
};

void HardwareSamplerTest::amdSnapshotReadsRealCapabilities()
{
    QTemporaryDir sys;
    QTemporaryDir proc;
    QVERIFY(sys.isValid());
    QVERIFY(proc.isValid());
    const QString card = sys.filePath(QStringLiteral("class/drm/card0"));
    const QString device = card + QStringLiteral("/device");
    QVERIFY(QDir().mkpath(sys.filePath(QStringLiteral("class/drm/card0-HDMI-A-1"))));
    writeFile(device + QStringLiteral("/vendor"), "0x1002\n");
    writeFile(device + QStringLiteral("/gpu_busy_percent"), "37\n");
    writeFile(device + QStringLiteral("/mem_info_vram_used"), "2192453632\n");
    writeFile(device + QStringLiteral("/mem_info_vram_total"), "8573157376\n");
    writeFile(device + QStringLiteral("/uevent"),
              "DRIVER=amdgpu\nPCI_SLOT_NAME=0000:0b:00.0\n");
    const QString driver = sys.filePath(QStringLiteral("bus/pci/drivers/amdgpu"));
    QVERIFY(QDir().mkpath(driver));
    QVERIFY(QFile::link(driver, device + QStringLiteral("/driver")));
    const QString hwmon = sys.filePath(QStringLiteral("class/hwmon/hwmon2"));
    writeFile(hwmon + QStringLiteral("/name"), "amdgpu\n");
    writeFile(hwmon + QStringLiteral("/device/uevent"),
              "PCI_SLOT_NAME=0000:0b:00.0\n");
    writeFile(hwmon + QStringLiteral("/temp1_input"), "50000\n");
    writeFile(hwmon + QStringLiteral("/temp1_label"), "edge\n");
    writeFile(hwmon + QStringLiteral("/power1_average"), "12000000\n");
    writeFile(hwmon + QStringLiteral("/power1_label"), "PPT\n");
    writeFile(hwmon + QStringLiteral("/fan1_input"), "1200\n");
    writeFile(hwmon + QStringLiteral("/in0_input"), "900\n");
    writeFile(proc.filePath(QStringLiteral("cpuinfo")),
              "model name\t: Fixture CPU\ncpu MHz\t: 1800.000\n");
    writeFile(sys.filePath(QStringLiteral("devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")),
              "1800000\n");

    const QVariantMap snapshot = HardwareSampler(sys.path(), proc.path()).sample();
    QCOMPARE(snapshot.value(QStringLiteral("gpus")).toList().size(), 1);
    const QVariantMap gpu = firstGpu(snapshot);
    QCOMPARE(gpu.value(QStringLiteral("id")).toString(), QStringLiteral("pci:0000:0b:00.0"));
    QCOMPARE(gpu.value(QStringLiteral("name")).toString(), QStringLiteral("AMD GPU"));
    QCOMPARE(gpu.value(QStringLiteral("driver")).toString(), QStringLiteral("amdgpu"));
    QCOMPARE(gpu.value(QStringLiteral("busy")).toLongLong(), 37LL);
    QCOMPARE(gpu.value(QStringLiteral("memoryUsed")).toLongLong(), 2192453632LL);
    QCOMPARE(gpu.value(QStringLiteral("memoryTotal")).toLongLong(), 8573157376LL);
    QCOMPARE(gpu.value(QStringLiteral("temperature")).toDouble(), 50.0);
    QCOMPARE(gpu.value(QStringLiteral("power")).toDouble(), 12.0);
    QVERIFY(gpu.value(QStringLiteral("unavailable")).toMap().contains(QStringLiteral("engines")));
    QCOMPARE(snapshot.value(QStringLiteral("cpuModel")).toString(), QStringLiteral("Fixture CPU"));
    QCOMPARE(snapshot.value(QStringLiteral("frequencyMHz")).toDouble(), 1800.0);
    const QVariantMap sensor = sensorById(snapshot.value(QStringLiteral("sensors")).toList(),
                                           QStringLiteral("pci:0000:0b:00.0/temp1_input"));
    QCOMPARE(sensor.value(QStringLiteral("name")).toString(), QStringLiteral("edge"));
    QCOMPARE(sensor.value(QStringLiteral("kind")).toString(), QStringLiteral("temperature"));
    QCOMPARE(sensor.value(QStringLiteral("unit")).toString(), QStringLiteral("C"));
    QCOMPARE(sensor.value(QStringLiteral("value")).toDouble(), 50.0);
}

void HardwareSamplerTest::malformedAndMissingValuesAreUnavailable()
{
    QTemporaryDir sys;
    QTemporaryDir proc;
    QVERIFY(sys.isValid());
    QVERIFY(proc.isValid());
    const QString device = sys.filePath(QStringLiteral("class/drm/card0/device"));
    writeFile(device + QStringLiteral("/vendor"), "0x1002\n");
    writeFile(device + QStringLiteral("/gpu_busy_percent"), "101\n");
    writeFile(device + QStringLiteral("/mem_info_vram_used"), "not-a-number\n");
    const HardwareSampler sampler(sys.path(), proc.path());
    const QVariantMap first = sampler.sample();
    const QVariantMap gpu = firstGpu(first);
    QVERIFY(!gpu.contains(QStringLiteral("busy")));
    QVERIFY(!gpu.contains(QStringLiteral("memoryUsed")));
    const QVariantMap unavailable = gpu.value(QStringLiteral("unavailable")).toMap();
    QCOMPARE(unavailable.value(QStringLiteral("busy")).toString(),
             QStringLiteral("gpu-busy-percent-unavailable"));
    QCOMPARE(unavailable.value(QStringLiteral("memoryUsed")).toString(),
             QStringLiteral("vram-used-unavailable"));
    QVERIFY(QDir(sys.filePath(QStringLiteral("class/drm/card0"))).removeRecursively());
    QCOMPARE(sampler.sample().value(QStringLiteral("gpus")).toList().size(), 0);
}

void HardwareSamplerTest::unsupportedGpuDoesNotClaimEngineOrTelemetryValues()
{
    QTemporaryDir sys;
    QTemporaryDir proc;
    QVERIFY(sys.isValid());
    QVERIFY(proc.isValid());
    const QString device = sys.filePath(QStringLiteral("class/drm/card1/device"));
    writeFile(device + QStringLiteral("/vendor"), "0x10de\n");
    writeFile(device + QStringLiteral("/uevent"), "PCI_SLOT_NAME=0000:03:00.0\n");
    const QVariantMap gpu = firstGpu(HardwareSampler(sys.path(), proc.path()).sample());
    QVERIFY(!gpu.contains(QStringLiteral("busy")));
    QVERIFY(!gpu.contains(QStringLiteral("memoryUsed")));
    QVERIFY(!gpu.contains(QStringLiteral("engines")));
    const QVariantMap unavailable = gpu.value(QStringLiteral("unavailable")).toMap();
    QCOMPARE(unavailable.value(QStringLiteral("nvidia")).toString(),
             QStringLiteral("nvidia-runtime-unavailable"));
    QCOMPARE(unavailable.value(QStringLiteral("engines")).toString(),
             QStringLiteral("engine-accounting-requires-device-context"));
}

void HardwareSamplerTest::cpuAndSensorsRemainAvailableWithoutDrm()
{
    QTemporaryDir sys;
    QTemporaryDir proc;
    QVERIFY(sys.isValid());
    QVERIFY(proc.isValid());
    writeFile(proc.filePath(QStringLiteral("cpuinfo")), "Hardware\t: Fixture ARM\n");
    writeFile(sys.filePath(QStringLiteral("class/hwmon/hwmon0/name")), "k10temp\n");
    writeFile(sys.filePath(QStringLiteral("class/hwmon/hwmon0/temp1_input")), "-500\n");
    const QVariantMap snapshot = HardwareSampler(sys.path(), proc.path()).sample();
    QCOMPARE(snapshot.value(QStringLiteral("gpus")).toList().size(), 0);
    QCOMPARE(snapshot.value(QStringLiteral("cpuModel")).toString(), QStringLiteral("Fixture ARM"));
    const QVariantMap sensor = sensorById(snapshot.value(QStringLiteral("sensors")).toList(),
                                           QStringLiteral("chip:k10temp/temp1_input"));
    QCOMPARE(sensor.value(QStringLiteral("value")).toDouble(), -0.5);
    QCOMPARE(sensor.value(QStringLiteral("unit")).toString(), QStringLiteral("C"));
}

void HardwareSamplerTest::multipleGpusKeepStableIdsAndUniqueSensors()
{
    QTemporaryDir sys;
    QTemporaryDir proc;
    QVERIFY(sys.isValid());
    QVERIFY(proc.isValid());
    const QString card0 = sys.filePath(QStringLiteral("class/drm/card0/device"));
    const QString card1 = sys.filePath(QStringLiteral("class/drm/card1/device"));
    writeFile(card0 + QStringLiteral("/vendor"), "0x1002\n");
    writeFile(card0 + QStringLiteral("/uevent"), "PCI_SLOT_NAME=0000:01:00.0\n");
    writeFile(card1 + QStringLiteral("/vendor"), "0x1002\n");
    writeFile(card1 + QStringLiteral("/uevent"), "PCI_SLOT_NAME=0000:02:00.0\n");
    const QString hwmon0 = sys.filePath(QStringLiteral("class/hwmon/hwmon0"));
    const QString hwmon1 = sys.filePath(QStringLiteral("class/hwmon/hwmon1"));
    writeFile(hwmon0 + QStringLiteral("/name"), "amdgpu\n");
    writeFile(hwmon0 + QStringLiteral("/temp1_input"), "40000\n");
    writeFile(hwmon1 + QStringLiteral("/name"), "amdgpu\n");
    writeFile(hwmon1 + QStringLiteral("/temp1_input"), "50000\n");
    QVERIFY(QFile::link(card0, hwmon0 + QStringLiteral("/device")));
    QVERIFY(QFile::link(card1, hwmon1 + QStringLiteral("/device")));
    const QVariantMap snapshot = HardwareSampler(sys.path(), proc.path()).sample();
    const QVariantList gpus = snapshot.value(QStringLiteral("gpus")).toList();
    QCOMPARE(gpus.size(), 2);
    QCOMPARE(gpus.at(0).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("pci:0000:01:00.0"));
    QCOMPARE(gpus.at(1).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("pci:0000:02:00.0"));
    const QVariantList sensors = snapshot.value(QStringLiteral("sensors")).toList();
    QCOMPARE(sensors.size(), 2);
    QCOMPARE(sensorById(sensors, QStringLiteral("pci:0000:01:00.0/temp1_input"))
                 .value(QStringLiteral("value"))
                 .toDouble(),
             40.0);
    QCOMPARE(sensorById(sensors, QStringLiteral("pci:0000:02:00.0/temp1_input"))
                 .value(QStringLiteral("value"))
                 .toDouble(),
             50.0);
}

QTEST_MAIN(HardwareSamplerTest)
#include "tst_hardware_sampler.moc"
