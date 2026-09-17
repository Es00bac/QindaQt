// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/backlight_writer.h>
#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>

#include "support/sysfs_backlight_fixture.h"

#include <QtCore/QFileInfo>
#include <QtTest>

#include <QtTest/QSignalSpy>

#include <memory>

using namespace QindaQt::Power;
using QindaQt::Tests::BacklightFixture;

namespace {

// A recording BacklightWriter. The sysfs source must not know or care that
// the real one talks to logind, so these rows need no bus at all.
class FakeBacklightWriter final : public Upstream::BacklightWriter
{
public:
    struct Call {
        QString deviceName;
        quint32 value = 0;
    };

    bool available() override
    {
        ++availableCalls;
        return isAvailable;
    }

    QString unavailableDiagnostic() const override { return diagnostic; }

    Upstream::BacklightWriteOutcome write(const QString &deviceName,
                                          const quint32 value) override
    {
        calls.push_back({deviceName, value});
        if (!isAvailable) {
            return {.status = Upstream::BacklightWriteStatus::Failed,
                    .reasonCode = diagnostic,
                    .diagnostic = {}};
        }
        if (refuse) {
            return {.status = Upstream::BacklightWriteStatus::Failed,
                    .reasonCode = QStringLiteral("logind-refused"),
                    .diagnostic = QStringLiteral("fake refusal")};
        }
        // The real writer changes the kernel value; the fixture stands in for
        // the kernel so the source's readback has something truthful to read.
        if (!kernelFile.isEmpty()) {
            BacklightFixture::write(kernelFile,
                                    QString::number(value).toUtf8() + '\n');
        }
        return {.status = Upstream::BacklightWriteStatus::Succeeded,
                .reasonCode = QStringLiteral("applied"),
                .diagnostic = {}};
    }

    QList<Call> calls;
    QString diagnostic = QStringLiteral("logind-unavailable");
    QString kernelFile;
    int availableCalls = 0;
    bool isAvailable = true;
    bool refuse = false;
};

Upstream::SysfsBacklightSource *startedSourceWithWriter(const QString &rootPath,
                                                        FakeBacklightWriter **writerOut)
{
    auto writer = std::make_unique<FakeBacklightWriter>();
    *writerOut = writer.get();
    auto *source =
        new Upstream::SysfsBacklightSource(rootPath, std::move(writer));
    source->start();
    return source;
}

} // namespace

// ADR-0186. A separate failure model from tst_power_sysfs_backlight.cpp: those
// rows prove what the source observes and writes on its own, these prove what
// it delegates when the kernel attribute is not writable by this process. On
// real laptop hardware that is every panel, so without these rows the
// Settings slider and the brightness keys are dead.
class PowerBacklightWriterSeamTests final : public QObject
{
    Q_OBJECT

private:
    // Scratch lives under the assigned build root, never the host /tmp.
    static QString scratchTemplate()
    {
        return QStringLiteral(QINDAQT_TEST_SCRATCH_DIR)
            + QStringLiteral("/writer-seam-XXXXXX");
    }

private Q_SLOTS:
    void readOnlyDeviceIsAdmittedWhenTheWriterIsAvailable();
    void readOnlyDeviceReportsTheWritersDiagnosticWhenUnavailable();
    void readOnlyWriteIsDelegatedAndReadBack();
    void delegatedRefusalIsReportedTruthfully();
    void writableDeviceNeverReachesTheWriter();
};

// ADR-0186. On a real laptop `brightness` is 0644 root:root, so without these
// rows every internal panel is published read-only and the Settings slider and
// the brightness keys are dead.
void PowerBacklightWriterSeamTests::readOnlyDeviceIsAdmittedWhenTheWriterIsAvailable()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    const QString directory =
        fixture.makeDevice(QStringLiteral("amdgpu_bl0"), "raw\n", "64764\n",
                           "64764\n", "64764\n");
    QFile brightnessFile(directory + QStringLiteral("/brightness"));
    const QFileDevice::Permissions original = brightnessFile.permissions();
    QVERIFY(brightnessFile.setPermissions(QFileDevice::ReadOwner));

    FakeBacklightWriter *writer = nullptr;
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        startedSourceWithWriter(root.path(), &writer));
    const InternalBacklight &device = source->devices().constFirst();
    QCOMPARE(device.status, BacklightStatus::Ok);
    QCOMPARE(device.reason, BacklightReason::None);
    QVERIFY(device.diagnostic.isEmpty());
    QVERIFY(device.observedKnown);
    QCOMPARE(device.observed, quint32(64764));
    brightnessFile.setPermissions(original);
}

void PowerBacklightWriterSeamTests::readOnlyDeviceReportsTheWritersDiagnosticWhenUnavailable()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    const QString directory =
        fixture.makeDevice(QStringLiteral("amdgpu_bl0"), "raw\n", "255\n", "200\n",
                           "200\n");
    QFile brightnessFile(directory + QStringLiteral("/brightness"));
    const QFileDevice::Permissions original = brightnessFile.permissions();
    QVERIFY(brightnessFile.setPermissions(QFileDevice::ReadOwner));

    auto writer = std::make_unique<FakeBacklightWriter>();
    writer->isAvailable = false;
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        new Upstream::SysfsBacklightSource(root.path(), std::move(writer)));
    source->start();
    const InternalBacklight &device = source->devices().constFirst();
    QCOMPARE(device.status, BacklightStatus::Unavailable);
    QCOMPARE(device.reason, BacklightReason::LogindError);
    // The reason the panel cannot be written survives to the Settings route.
    QCOMPARE(device.diagnostic, QStringLiteral("logind-unavailable"));
    brightnessFile.setPermissions(original);
}

void PowerBacklightWriterSeamTests::readOnlyWriteIsDelegatedAndReadBack()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    const QString directory =
        fixture.makeDevice(QStringLiteral("amdgpu_bl0"), "raw\n", "64764\n",
                           "64764\n", "64764\n");
    QFile brightnessFile(directory + QStringLiteral("/brightness"));
    const QFileDevice::Permissions original = brightnessFile.permissions();
    QVERIFY(brightnessFile.setPermissions(QFileDevice::ReadOwner));

    FakeBacklightWriter *writer = nullptr;
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        startedSourceWithWriter(root.path(), &writer));
    // The fixture stands in for the kernel: the delegated write lands in
    // actual_brightness, which is what the source must re-read.
    writer->kernelFile = directory + QStringLiteral("/actual_brightness");
    const QString opaqueId = source->devices().constFirst().handle.opaqueId;

    QSignalSpy changed(source.get(), &Upstream::SysfsBacklightSource::devicesChanged);
    const Upstream::BacklightWriteOutcome outcome =
        source->writeBrightness(opaqueId, 30000);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(outcome.reasonCode, QStringLiteral("applied"));
    QCOMPARE(writer->calls.size(), 1);
    // The writer is handed the sysfs device name and the raw kernel value,
    // never a path.
    QCOMPARE(writer->calls.constFirst().deviceName, QStringLiteral("amdgpu_bl0"));
    QCOMPARE(writer->calls.constFirst().value, quint32(30000));
    // The unwritable kernel attribute was not touched by this process.
    QCOMPARE(BacklightFixture::read(directory + QStringLiteral("/brightness")),
             QByteArrayLiteral("64764\n"));
    QTRY_COMPARE(changed.size(), 1);
    QCOMPARE(source->devices().constFirst().observed, quint32(30000));

    // Range and identity checks still run before delegation.
    QCOMPARE(source->writeBrightness(opaqueId, 64765).status,
             Upstream::BacklightWriteStatus::Rejected);
    QCOMPARE(writer->calls.size(), 1);
    brightnessFile.setPermissions(original);
}

void PowerBacklightWriterSeamTests::delegatedRefusalIsReportedTruthfully()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    const QString directory =
        fixture.makeDevice(QStringLiteral("amdgpu_bl0"), "raw\n", "255\n", "200\n",
                           "200\n");
    QFile brightnessFile(directory + QStringLiteral("/brightness"));
    const QFileDevice::Permissions original = brightnessFile.permissions();
    QVERIFY(brightnessFile.setPermissions(QFileDevice::ReadOwner));

    FakeBacklightWriter *writer = nullptr;
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        startedSourceWithWriter(root.path(), &writer));
    writer->refuse = true;
    const QString opaqueId = source->devices().constFirst().handle.opaqueId;
    const Upstream::BacklightWriteOutcome outcome =
        source->writeBrightness(opaqueId, 100);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Failed);
    QCOMPARE(outcome.reasonCode, QStringLiteral("logind-refused"));
    QCOMPARE(BacklightFixture::read(directory + QStringLiteral("/brightness")),
             QByteArrayLiteral("200\n"));
    brightnessFile.setPermissions(original);
}

void PowerBacklightWriterSeamTests::writableDeviceNeverReachesTheWriter()
{
    QTemporaryDir root{scratchTemplate()};
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel"), "firmware\n", "255\n", "100\n",
                       "100\n");

    FakeBacklightWriter *writer = nullptr;
    std::unique_ptr<Upstream::SysfsBacklightSource> source(
        startedSourceWithWriter(root.path(), &writer));
    const QString opaqueId = source->devices().constFirst().handle.opaqueId;
    QCOMPARE(source->writeBrightness(opaqueId, 127).status,
             Upstream::BacklightWriteStatus::Succeeded);
    // AGENT-GUARD: delegation is a fallback, not a replacement. A writable
    // attribute must keep the direct, dependency-free path.
    QVERIFY(writer->calls.isEmpty());
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/panel/brightness")),
             QByteArrayLiteral("127\n"));
}

QTEST_MAIN(PowerBacklightWriterSeamTests)
#include "tst_power_backlight_writer_seam.moc"
