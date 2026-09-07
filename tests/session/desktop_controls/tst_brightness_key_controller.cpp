// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>
#include <qindaqt/session/desktop_controls/brightness_key_controller.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Session::DesktopControls;

namespace {

struct BacklightFixture
{
    explicit BacklightFixture(const QTemporaryDir &root) : rootPath(root.path()) {}

    QString makeDevice(const QString &name, const QByteArray &type,
                       const QByteArray &maximum, const QByteArray &brightness)
    {
        const QString directory = rootPath + QLatin1Char('/') + name;
        if (!QDir().mkpath(directory)) {
            return {};
        }
        write(directory + QStringLiteral("/type"), type);
        write(directory + QStringLiteral("/max_brightness"), maximum);
        write(directory + QStringLiteral("/brightness"), brightness);
        write(directory + QStringLiteral("/actual_brightness"), brightness);
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
        return file.readAll().trimmed();
    }

    QString rootPath;
};

} // namespace

class BrightnessKeyControllerTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void raiseWritesOneSixteenthStepAndReportsPercent();
    void lowerClampsAtOne();
    void raiseClampsAtMaximum();
    void firmwareDeviceIsPreferredOverPlatform();
    void missingDevicesReportUnavailable();
    void readOnlyDeviceReportsTypedUnavailable();

private:
    std::unique_ptr<Upstream::SysfsBacklightSource> startedSource(const QString &root)
    {
        auto source = std::make_unique<Upstream::SysfsBacklightSource>(root);
        source->start();
        return source;
    }
};

void BrightnessKeyControllerTest::raiseWritesOneSixteenthStepAndReportsPercent() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel0"), QByteArrayLiteral("firmware"),
                       QByteArrayLiteral("1600"), QByteArrayLiteral("800"));
    auto source = startedSource(root.path());
    QCOMPARE(source->devices().size(), 1);

    BrightnessKeyController controller(*source);
    QSignalSpy feedback(&controller, &BrightnessKeyController::brightnessFeedbackRequested);
    QSignalSpy unavailable(&controller, &BrightnessKeyController::brightnessUnavailable);
    controller.raiseBrightness();

    QCOMPARE(unavailable.count(), 0);
    QCOMPARE(feedback.count(), 1);
    QCOMPARE(feedback.at(0).at(0).toInt(), 56);
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/panel0/brightness")),
             QByteArrayLiteral("900"));
}

void BrightnessKeyControllerTest::lowerClampsAtOne() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel0"), QByteArrayLiteral("firmware"),
                       QByteArrayLiteral("1500"), QByteArrayLiteral("10"));
    auto source = startedSource(root.path());

    BrightnessKeyController controller(*source);
    QSignalSpy feedback(&controller, &BrightnessKeyController::brightnessFeedbackRequested);
    controller.lowerBrightness();

    QCOMPARE(feedback.at(0).at(0).toInt(), 0);
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/panel0/brightness")),
             QByteArrayLiteral("1"));
}

void BrightnessKeyControllerTest::raiseClampsAtMaximum() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel0"), QByteArrayLiteral("firmware"),
                       QByteArrayLiteral("1500"), QByteArrayLiteral("1500"));
    auto source = startedSource(root.path());

    BrightnessKeyController controller(*source);
    QSignalSpy feedback(&controller, &BrightnessKeyController::brightnessFeedbackRequested);
    controller.raiseBrightness();

    QCOMPARE(feedback.at(0).at(0).toInt(), 100);
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/panel0/brightness")),
             QByteArrayLiteral("1500"));
}

void BrightnessKeyControllerTest::firmwareDeviceIsPreferredOverPlatform() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("platform0"), QByteArrayLiteral("platform"),
                       QByteArrayLiteral("100"), QByteArrayLiteral("50"));
    fixture.makeDevice(QStringLiteral("firmware0"), QByteArrayLiteral("firmware"),
                       QByteArrayLiteral("1000"), QByteArrayLiteral("500"));
    auto source = startedSource(root.path());
    QCOMPARE(source->devices().size(), 2);

    BrightnessKeyController controller(*source);
    QSignalSpy feedback(&controller, &BrightnessKeyController::brightnessFeedbackRequested);
    controller.raiseBrightness();

    QCOMPARE(feedback.at(0).at(0).toInt(), 56);
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/firmware0/brightness")),
             QByteArrayLiteral("562"));
    // The unpreferred platform device is untouched.
    QCOMPARE(BacklightFixture::read(root.path() + QStringLiteral("/platform0/brightness")),
             QByteArrayLiteral("50"));
}

void BrightnessKeyControllerTest::missingDevicesReportUnavailable() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto source = startedSource(root.path());

    BrightnessKeyController controller(*source);
    QSignalSpy unavailable(&controller, &BrightnessKeyController::brightnessUnavailable);
    QSignalSpy feedback(&controller, &BrightnessKeyController::brightnessFeedbackRequested);
    controller.raiseBrightness();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("no-backlight-device"));
    QCOMPARE(feedback.count(), 0);
}

void BrightnessKeyControllerTest::readOnlyDeviceReportsTypedUnavailable() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    BacklightFixture fixture(root);
    fixture.makeDevice(QStringLiteral("panel0"), QByteArrayLiteral("firmware"),
                       QByteArrayLiteral("1500"), QByteArrayLiteral("500"));
    QFile brightness(root.path() + QStringLiteral("/panel0/brightness"));
    QVERIFY(brightness.setPermissions(QFileDevice::ReadUser));

    auto source = startedSource(root.path());
    BrightnessKeyController controller(*source);
    QSignalSpy unavailable(&controller, &BrightnessKeyController::brightnessUnavailable);
    QSignalSpy feedback(&controller, &BrightnessKeyController::brightnessFeedbackRequested);
    controller.raiseBrightness();

    QCOMPARE(unavailable.count(), 1);
    QCOMPARE(unavailable.at(0).at(0).toString(), QStringLiteral("backlight-read-only"));
    QCOMPARE(feedback.count(), 0);
}

QTEST_MAIN(BrightnessKeyControllerTest)
#include "tst_brightness_key_controller.moc"
