// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session/desktop_controls/powerdevil_brightness_feedback_observer.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusError>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

using QindaQt::Session::DesktopControls::PowerDevilBrightnessFeedbackObserver;

namespace {

constexpr auto Service = "org.kde.ScreenBrightness";
constexpr auto RootPath = "/org/kde/ScreenBrightness";
constexpr auto RootInterface = "org.kde.ScreenBrightness";
constexpr auto DisplayPath = "/org/kde/ScreenBrightness/display1";
constexpr auto DisplayInterface = "org.kde.ScreenBrightness.Display";

class FakeDisplay final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ScreenBrightness.Display")
    Q_PROPERTY(int MaxBrightness READ maxBrightness)

public:
    int maxBrightness() const { return m_maxBrightness; }
    void setMaxBrightness(int maximum) { m_maxBrightness = maximum; }

private:
    int m_maxBrightness = 1000;
};

class FakeScreenBrightness final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ScreenBrightness")

public:
    void emitBrightness(int brightness, const QString &source,
                        const QString &context)
    {
        Q_EMIT BrightnessChanged(QStringLiteral("display1"), brightness, source,
                                 context);
    }

Q_SIGNALS:
    void BrightnessChanged(const QString &displayName, int brightness,
                           const QString &sourceClientName,
                           const QString &sourceClientContext);
};

bool registerService(QDBusConnection &bus, FakeScreenBrightness &root,
                     FakeDisplay &display)
{
    const bool service = bus.registerService(QString::fromLatin1(Service));
    const bool rootObject = bus.registerObject(QString::fromLatin1(RootPath), &root,
                                               QDBusConnection::ExportAllSignals);
    const bool displayObject = bus.registerObject(
        QString::fromLatin1(DisplayPath), &display,
        QDBusConnection::ExportAllProperties);
    return service && rootObject && displayObject;
}

void unregisterService(QDBusConnection &bus)
{
    bus.unregisterObject(QString::fromLatin1(DisplayPath));
    bus.unregisterObject(QString::fromLatin1(RootPath));
    bus.unregisterService(QString::fromLatin1(Service));
}

} // namespace

class PowerDevilBrightnessFeedbackObserverTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void keyFeedbackIsNormalizedFromDisplayRange();
    void nonKeyChangesAreIgnored();
    void invalidRangesAreIgnored();
    void ownerAvailabilityIsTracked();
};

void PowerDevilBrightnessFeedbackObserverTest::keyFeedbackIsNormalizedFromDisplayRange()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakeScreenBrightness root;
    FakeDisplay display;
    QVERIFY(registerService(bus, root, display));
    PowerDevilBrightnessFeedbackObserver observer(bus);
    QSignalSpy feedback(&observer,
                        &PowerDevilBrightnessFeedbackObserver::brightnessFeedbackRequested);
    observer.start();
    QTRY_VERIFY(observer.available());

    root.emitBrightness(500, QStringLiteral("(internal)"),
                        QStringLiteral("brightness_key"));
    QTRY_COMPARE(feedback.count(), 1);
    QCOMPARE(feedback.at(0).at(0).toInt(), 50);
    unregisterService(bus);
}

void PowerDevilBrightnessFeedbackObserverTest::nonKeyChangesAreIgnored()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakeScreenBrightness root;
    FakeDisplay display;
    QVERIFY(registerService(bus, root, display));
    PowerDevilBrightnessFeedbackObserver observer(bus);
    QSignalSpy feedback(&observer,
                        &PowerDevilBrightnessFeedbackObserver::brightnessFeedbackRequested);
    observer.start();
    QTRY_VERIFY(observer.available());

    root.emitBrightness(500, QStringLiteral("(internal)"), QString{});
    root.emitBrightness(500, QStringLiteral("other-client"),
                        QStringLiteral("brightness_key"));
    QTest::qWait(100);
    QCOMPARE(feedback.count(), 0);
    unregisterService(bus);
}

void PowerDevilBrightnessFeedbackObserverTest::invalidRangesAreIgnored()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakeScreenBrightness root;
    FakeDisplay display;
    QVERIFY(registerService(bus, root, display));
    PowerDevilBrightnessFeedbackObserver observer(bus);
    QSignalSpy feedback(&observer,
                        &PowerDevilBrightnessFeedbackObserver::brightnessFeedbackRequested);
    observer.start();
    QTRY_VERIFY(observer.available());

    root.emitBrightness(-1, QStringLiteral("(internal)"),
                        QStringLiteral("brightness_key"));
    root.emitBrightness(1001, QStringLiteral("(internal)"),
                        QStringLiteral("brightness_key"));
    display.setMaxBrightness(0);
    root.emitBrightness(0, QStringLiteral("(internal)"),
                        QStringLiteral("brightness_key"));
    QTest::qWait(100);
    QCOMPARE(feedback.count(), 0);
    unregisterService(bus);
}

void PowerDevilBrightnessFeedbackObserverTest::ownerAvailabilityIsTracked()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PowerDevilBrightnessFeedbackObserver observer(bus);
    QSignalSpy feedback(&observer,
                        &PowerDevilBrightnessFeedbackObserver::brightnessFeedbackRequested);
    observer.start();
    QVERIFY(!observer.available());

    FakeScreenBrightness root;
    FakeDisplay display;
    QVERIFY(registerService(bus, root, display));
    QTRY_VERIFY(observer.available());
    unregisterService(bus);
    QTRY_VERIFY(!observer.available());
    QCOMPARE(feedback.count(), 0);
}

QTEST_MAIN(PowerDevilBrightnessFeedbackObserverTest)
#include "tst_powerdevil_brightness_feedback_observer.moc"
