// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_types.h>
#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <QtTest/QSignalSpy>

#include <memory>

using namespace QindaQt::Power;

namespace {

struct BacklightFixture
{
    explicit BacklightFixture(const QTemporaryDir &root)
        : rootPath(root.path())
    {
    }

    QString makeDevice(const QString &name, const QByteArray &type,
                       const QByteArray &maximum, const QByteArray &brightness,
                       const QByteArray &actual = QByteArray())
    {
        const QString directory = rootPath + QLatin1Char('/') + name;
        if (!QDir().mkpath(directory)) {
            return {};
        }
        write(directory + QStringLiteral("/type"), type);
        write(directory + QStringLiteral("/max_brightness"), maximum);
        write(directory + QStringLiteral("/brightness"), brightness);
        if (!actual.isNull()) {
            write(directory + QStringLiteral("/actual_brightness"), actual);
        }
        return directory;
    }

    static void write(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
        file.close();
    }

    static QByteArray read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    QString rootPath;
};

Upstream::SysfsBacklightSource *startedSource(const QString &rootPath,
                                              QSignalSpy **spyOut = nullptr)
{
    auto *source = new Upstream::SysfsBacklightSource(rootPath);
    auto *spy = new QSignalSpy(source, &Upstream::SysfsBacklightSource::devicesChanged);
    source->start();
    if (spyOut != nullptr) {
        *spyOut = spy;
    }
    return source;
}

} // namespace

class PowerSysfsBacklightTests final : public QObject
{
    Q_OBJECT

private:
    // Scratch lives under the assigned build root, never the host /tmp.
    static QString scratchTemplate()
    {
        return QStringLiteral(QINDAQT_TEST_SCRATCH_DIR)
            + QStringLiteral("/sysfs-XXXXXX");
    }

private Q_SLOTS:
    void enumeratesFirmwareDeviceWithExactRawValues();
    void classifiesPlatformAndRawKinds();
    void fallsBackToBrightnessWhenActualIsMissing();
    void hostileMaxBrightnessPublishesTypedUnavailable();
    void hostileCurrentPublishesDegradedTruth();
    void observedAboveMaximumPublishesDegradedTruth();
    void unclassifiableDeviceIsDropped();
    void deviceDisappearanceAndAppearanceUpdateList();
    void writeSucceedsOnWritableRoot();
    void writeRejectsOutOfRangeAndUnknownDevices();
    void writeFailsClosedOnReadOnlyRoot();
    void readOnlyRootPublishesUnavailableTruth();
    void missingRootPublishesEmptyTruth();
    void longButLegalDeviceNameIsPreserved();
};

void PowerSysfsBacklightTests::enumeratesFirmwareDeviceWithExactRawValues()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("intel_backlight"), "firmware\n", "255\n",
                       "128\n", "130\n");

    QSignalSpy *spy = nullptr;
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        startedSource(root.path(), &spy));
    QTRY_COMPARE(spy->size(), 1);
    const QList<InternalBacklight> devices = source->devices();
    QCOMPARE(devices.size(), 1);
    const InternalBacklight &device = devices.constFirst();
    QCOMPARE(device.deviceName, QStringLiteral("intel_backlight"));
    QVERIFY(device.internal);
    QCOMPARE(device.kind, BacklightKind::Firmware);
    QCOMPARE(device.maximum, quint32(255));
    QVERIFY(device.observedKnown);
    QCOMPARE(device.observed, quint32(130));
    QCOMPARE(device.status, BacklightStatus::Ok);
    QCOMPARE(device.reason, BacklightReason::None);
    QVERIFY(device.diagnostic.isEmpty());
    // The opaque ID is a bounded derivation, not the sysfs name or path.
    QCOMPARE(device.handle.opaqueId.size(), 32);
    QVERIFY(device.handle.opaqueId != QStringLiteral("intel_backlight"));
}

void PowerSysfsBacklightTests::classifiesPlatformAndRawKinds()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("acpi_video0"), "platform\n", "100\n", "40\n",
                       "41\n");
    fixture.makeDevice(QStringLiteral("raw_panel"), "raw\n", "10\n", "3\n", "3\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    QCOMPARE(source->devices().size(), 2);
    QCOMPARE(source->devices().at(0).kind, BacklightKind::Platform);
    QCOMPARE(source->devices().at(1).kind, BacklightKind::Raw);
}

void PowerSysfsBacklightTests::fallsBackToBrightnessWhenActualIsMissing()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("nvidia_0"), "raw\n", "1000\n", "640\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    QCOMPARE(source->devices().size(), 1);
    const InternalBacklight &device = source->devices().constFirst();
    QCOMPARE(device.status, BacklightStatus::Ok);
    QVERIFY(device.observedKnown);
    QCOMPARE(device.observed, quint32(640));
}

void PowerSysfsBacklightTests::hostileMaxBrightnessPublishesTypedUnavailable()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("broken"), "platform\n", "not-a-number\n",
                       "5\n", "5\n");
    fixture.makeDevice(QStringLiteral("zero"), "platform\n", "0\n", "0\n", "0\n");
    fixture.makeDevice(QStringLiteral("oversize"), "platform\n",
                       QByteArrayLiteral("2000000000\n"), "5\n", "5\n");
    fixture.makeDevice(QStringLiteral("negative"), "platform\n", "-5\n", "5\n", "5\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    // Sorted by directory name: broken, negative, oversize, zero.
    const QList<InternalBacklight> devices = source->devices();
    QCOMPARE(devices.size(), 4);
    for (const InternalBacklight &device : devices) {
        QCOMPARE(device.status, BacklightStatus::Unavailable);
        QCOMPARE(device.reason, BacklightReason::NoBacklight);
        QCOMPARE(device.maximum, quint32(0));
        QVERIFY(!device.observedKnown);
        QCOMPARE(device.observed, quint32(0));
        QVERIFY(!device.diagnostic.isEmpty());
    }
}

void PowerSysfsBacklightTests::hostileCurrentPublishesDegradedTruth()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("texty"), "platform\n", "100\n", "banana\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    const QList<InternalBacklight> devices = source->devices();
    QCOMPARE(devices.size(), 1);
    const InternalBacklight &device = devices.constFirst();
    QCOMPARE(device.status, BacklightStatus::Degraded);
    QCOMPARE(device.reason, BacklightReason::DeviceDisappeared);
    QCOMPARE(device.maximum, quint32(100));
    QVERIFY(!device.observedKnown);
    QCOMPARE(device.observed, quint32(0));
    QCOMPARE(device.diagnostic, QStringLiteral("current-brightness-unreadable"));
}

void PowerSysfsBacklightTests::observedAboveMaximumPublishesDegradedTruth()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("lying"), "platform\n", "255\n", "300\n",
                       "300\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    const InternalBacklight &device = source->devices().constFirst();
    QCOMPARE(device.status, BacklightStatus::Degraded);
    QCOMPARE(device.reason, BacklightReason::DeviceDisappeared);
    QCOMPARE(device.maximum, quint32(255));
    QVERIFY(!device.observedKnown);
    QCOMPARE(device.diagnostic, QStringLiteral("observed-exceeds-maximum"));
}

void PowerSysfsBacklightTests::unclassifiableDeviceIsDropped()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("weird"), "alien\n", "100\n", "5\n", "5\n");
    QDir(root.path()).mkpath(QStringLiteral("typless"));
    BacklightFixture::write(root.path() + QStringLiteral("/typless/max_brightness"),
                            "100\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    QVERIFY(source->devices().isEmpty());
}

void PowerSysfsBacklightTests::deviceDisappearanceAndAppearanceUpdateList()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("first"), "platform\n", "100\n", "5\n", "5\n");
    fixture.makeDevice(QStringLiteral("second"), "platform\n", "90\n", "9\n", "9\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    QTRY_COMPARE(source->devices().size(), 2);
    QSignalSpy changed(source.get(), &Upstream::SysfsBacklightSource::devicesChanged);

    QVERIFY(QDir(root.path() + QStringLiteral("/first")).removeRecursively());
    QTRY_COMPARE(source->devices().size(), 1);
    QCOMPARE(source->devices().constFirst().deviceName, QStringLiteral("second"));
    QVERIFY(changed.size() >= 1);

    fixture.makeDevice(QStringLiteral("third"), "raw\n", "10\n", "1\n", "1\n");
    QTRY_COMPARE(source->devices().size(), 2);
}

void PowerSysfsBacklightTests::writeSucceedsOnWritableRoot()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel"), "firmware\n", "255\n", "100\n",
                       "100\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    const QString opaqueId = source->devices().constFirst().handle.opaqueId;
    QSignalSpy changed(source.get(), &Upstream::SysfsBacklightSource::devicesChanged);
    const Upstream::BacklightWriteOutcome outcome =
        source->writeBrightness(opaqueId, 127);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(outcome.reasonCode, QStringLiteral("applied"));
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/panel/brightness")),
             QByteArrayLiteral("127\n"));
    const InternalBacklight &device = source->devices().constFirst();
    QVERIFY(device.observedKnown);
    QCOMPARE(device.observed, quint32(100));
    QCOMPARE(device.status, BacklightStatus::Ok);
    QTRY_COMPARE(changed.size(), 1);
}

void PowerSysfsBacklightTests::writeRejectsOutOfRangeAndUnknownDevices()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel"), "firmware\n", "255\n", "100\n",
                       "100\n");
    fixture.makeDevice(QStringLiteral("dead"), "firmware\n", "zero-ish\n", "1\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    QString panelId;
    QString deadId;
    for (const InternalBacklight &device : source->devices()) {
        if (device.deviceName == QStringLiteral("panel")) {
            panelId = device.handle.opaqueId;
        }
        if (device.deviceName == QStringLiteral("dead")) {
            deadId = device.handle.opaqueId;
        }
    }
    QVERIFY(!panelId.isEmpty());
    QVERIFY(!deadId.isEmpty());

    QCOMPARE(source->writeBrightness(panelId, 256).status,
             Upstream::BacklightWriteStatus::Rejected);
    QCOMPARE(source->writeBrightness(panelId, 256).reasonCode,
             QStringLiteral("value-out-of-range"));
    QCOMPARE(source->writeBrightness(QStringLiteral("does-not-exist"), 1).status,
             Upstream::BacklightWriteStatus::Rejected);
    QCOMPARE(source->writeBrightness(deadId, 1).status,
             Upstream::BacklightWriteStatus::Unsupported);
    QCOMPARE(source->writeBrightness(deadId, 1).reasonCode,
             QStringLiteral("backlight-unavailable"));
}

void PowerSysfsBacklightTests::writeFailsClosedOnReadOnlyRoot()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    const QString directory =
        fixture.makeDevice(QStringLiteral("locked"), "firmware\n", "255\n", "10\n",
                           "10\n");
    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    const QString opaqueId = source->devices().constFirst().handle.opaqueId;

    QFile brightnessFile(directory + QStringLiteral("/brightness"));
    const QFileDevice::Permissions original = brightnessFile.permissions();
    QVERIFY(brightnessFile.setPermissions(QFileDevice::ReadOwner));
    const Upstream::BacklightWriteOutcome outcome =
        source->writeBrightness(opaqueId, 50);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Failed);
    QCOMPARE(outcome.reasonCode, QStringLiteral("backlight-read-only"));
    QVERIFY(!outcome.diagnostic.isEmpty());
    QCOMPARE(BacklightFixture::read(directory + QStringLiteral("/brightness")),
             QByteArrayLiteral("10\n"));
    brightnessFile.setPermissions(original);
}

void PowerSysfsBacklightTests::readOnlyRootPublishesUnavailableTruth()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    const QString directory =
        fixture.makeDevice(QStringLiteral("locked"), "firmware\n", "255\n", "10\n",
                           "10\n");
    QFile brightnessFile(directory + QStringLiteral("/brightness"));
    const QFileDevice::Permissions original = brightnessFile.permissions();
    QVERIFY(brightnessFile.setPermissions(QFileDevice::ReadOwner));

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    const InternalBacklight &device = source->devices().constFirst();
    QCOMPARE(device.status, BacklightStatus::Unavailable);
    QCOMPARE(device.reason, BacklightReason::LogindError);
    QCOMPARE(device.diagnostic, QStringLiteral("backlight-read-only"));
    QVERIFY(device.observedKnown);
    QCOMPARE(device.observed, quint32(10));
    brightnessFile.setPermissions(original);
}

void PowerSysfsBacklightTests::missingRootPublishesEmptyTruth()
{
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        startedSource(QStringLiteral("/nonexistent-qindaqt-backlight-root")));
    QVERIFY(source->devices().isEmpty());
    QCOMPARE(source->writeBrightness(QStringLiteral("any"), 1).status,
             Upstream::BacklightWriteStatus::Rejected);
}

void PowerSysfsBacklightTests::longButLegalDeviceNameIsPreserved()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    // Filesystem names cap at 255 bytes, inside the 256-byte public bound, so
    // any legal device name is representable without truncation.
    const QString longName = QString(QStringLiteral("d")).repeated(200);
    fixture.makeDevice(longName, "platform\n", "100\n", "1\n", "1\n");

    std::unique_ptr<Upstream::SysfsBacklightSource> source(startedSource(root.path()));
    QCOMPARE(source->devices().size(), 1);
    QCOMPARE(source->devices().constFirst().deviceName, longName);
}

QTEST_GUILESS_MAIN(PowerSysfsBacklightTests)
#include "tst_power_sysfs_backlight.moc"
