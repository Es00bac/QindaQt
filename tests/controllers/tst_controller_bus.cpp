// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controller_port.h"
#include "qindaqt/controllers/controller_policy.h"
#include "voice_button.h"
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QtTest>
using namespace QindaQt::Apps::SettingsInput;
using namespace QindaQt::Controllers;
class ControllerFixture final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Controllers1")
    quint64 revision = 1;
public Q_SLOTS:
    QString GetSnapshot() {
        const auto before = revision;
        if (revision == 1) { ++revision; Q_EMIT Changed(revision); }
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"revision", QString::number(before)}, {"controllers", QJsonArray{}}}).toJson());
    }
    QString Apply(const QString &, const QString &, qulonglong expected) {
        const bool ok = expected == revision;
        if (ok) ++revision;
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"ok", ok}, {"reason", ok ? "ok" : "revision-stale"}, {"revision", QString::number(revision)}}).toJson());
    }
Q_SIGNALS:
    void Changed(qulonglong revision);
};
class VoiceFixture final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Voice1")
    int starts = 0, finishes = 0;
    quint64 revision = 1;
public Q_SLOTS:
    QVariantMap GetSnapshot() { return {{"state", starts > finishes ? 3 : 1}, {"enabled", true}, {"revision", QVariant::fromValue<qulonglong>(revision)}}; }
    QVariantMap StartDictation(qulonglong, qulonglong expected) {
        if (expected != revision) return {{"status", 1}};
        ++starts; ++revision; return {{"status", 0}};
    }
    QVariantMap Finish(qulonglong, qulonglong expected) {
        if (expected != revision) return {{"status", 1}};
        ++finishes; ++revision; return {{"status", 0}};
    }
    QVariantMap Cancel(qulonglong id, qulonglong expected) { return Finish(id, expected); }
    int Starts() const { return starts; }
    int Finishes() const { return finishes; }
};
class ControllerBusTest final : public QObject {
    Q_OBJECT
    QProcess fixture;
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").contains("unix:"));
        fixture.start(QCoreApplication::applicationFilePath(), {"--fixture"});
        QVERIFY(fixture.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QDBusInterface(Service, Object, Interface).isValid(), 2000);
    }
    void changedDuringSnapshotIsNotDroppedAndCompletionFollowsReadback() {
        QtControllerPort port(QDBusConnection::sessionBus());
        quint64 seen = 0;
        connect(&port, &ControllerPort::snapshotReceived, this, [&](const QJsonObject &s) { seen = s.value("revision").toString().toULongLong(); });
        port.refresh(); QTRY_COMPARE_WITH_TIMEOUT(seen, quint64(2), 2000);
        quint64 whenCompleted = 0; bool success = false;
        connect(&port, &ControllerPort::completed, this, [&](bool ok, const QString &) { success = ok; whenCompleted = seen; });
        port.apply("test", {{"gyro", true}}, seen);
        QTRY_VERIFY_WITH_TIMEOUT(success, 2000); QCOMPARE(whenCompleted, quint64(3));
    }
    void dictationResolvesOwnerAndReleaseFinishesCapture() {
        VoiceButton button; QSignalSpy failures(&button, &VoiceButton::failed);
        QDBusInterface service("org.qindaqt.Voice1", "/org/qindaqt/Voice1", "org.qindaqt.Voice1");
        button.press("controller/back");
        QTRY_COMPARE_WITH_TIMEOUT(service.call("Starts").arguments().value(0).toInt(), 1, 2000);
        button.release("controller/back");
        QTRY_COMPARE_WITH_TIMEOUT(service.call("Finishes").arguments().value(0).toInt(), 1, 2000);
        QCOMPARE(failures.count(), 0);
    }
    void cleanupTestCase() { fixture.terminate(); fixture.waitForFinished(2000); }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--fixture")) {
        ControllerFixture controller; VoiceFixture voice;
        auto bus = QDBusConnection::sessionBus();
        const auto flags = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals;
        if (!bus.registerService(Service) || !bus.registerObject(Object, &controller, flags)
            || !bus.registerService("org.qindaqt.Voice1") || !bus.registerObject("/org/qindaqt/Voice1", &voice, flags)) return 2;
        return app.exec();
    }
    ControllerBusTest test; return QTest::qExec(&test, argc, argv);
}
#include "tst_controller_bus.moc"
