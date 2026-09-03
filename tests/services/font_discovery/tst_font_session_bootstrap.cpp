// SPDX-License-Identifier: GPL-3.0-or-later
//
// AGENT-NOTE: Rows for the F1 session bootstrap composition root (review
// findings P1-1, P1-4, P1-6 of rejected candidate abc76f3). Child-process and
// fake-service machinery lives in font_session_bootstrap_test_support.h; the
// probe mode runs applyFromSessionSettings() before QGuiApplication
// construction, exactly like a first-party main().

#include "font_session_bootstrap_test_support.h"

#include <QDBusReply>
#include <QDir>
#include <QPair>
#include <QScopeGuard>

using namespace FontSessionBootstrapTestSupport;

class FontSessionBootstrapTests final : public QObject {
    Q_OBJECT
private slots:
    void readWithoutConnectionFailsClosed();
    void readWithoutServiceFailsClosed();
    void probeWithoutBusAddressFailsFastWithoutAutolaunch();
    void probeWithAbsentBusFailsFast();
    void probeWithoutServiceChangesNothing();
    void probeWithWrongTypedSnapshotChangesNothing();
    void probeWithMalformedEnvelopeChangesNothing();
    void probeWithUnresolvedFamilyChangesNothing();
    void probeAppliesConfirmedPreferencesFromFakeService();
    void probeAppliesConfirmedPreferencesFromRealService();
};

void FontSessionBootstrapTests::readWithoutConnectionFailsClosed()
{
    const QDBusConnection unconnected(QStringLiteral("qindaqt-font-bootstrap-invalid"));
    QString diagnostic;
    const auto preferences =
        FontSessionBootstrap::readConfirmedPreferences(unconnected, 500, &diagnostic);
    QVERIFY(!preferences.has_value());
    QVERIFY(!diagnostic.isEmpty());
}

void FontSessionBootstrapTests::readWithoutServiceFailsClosed()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QElapsedTimer timer;
    timer.start();
    const auto preferences =
        FontSessionBootstrap::readConfirmedPreferences(bus.connection(), 1'500);
    QVERIFY(!preferences.has_value());
    QVERIFY(timer.elapsed() < 10'000);
}

void FontSessionBootstrapTests::probeWithoutBusAddressFailsFastWithoutAutolaunch()
{
    // AGENT-GUARD: an unset bus address means "no preference source"; the
    // helper must never autolaunch a session bus.
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const ProbeOutcome outcome = runProbeChild(baseChildEnvironment(scratch.path()));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(!outcome.applied);
    QVERIFY(outcome.elapsed >= 0 && outcome.elapsed < 5'000);
}

void FontSessionBootstrapTests::probeWithAbsentBusFailsFast()
{
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    QProcessEnvironment environment = baseChildEnvironment(scratch.path());
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                       QStringLiteral("unix:path=/nonexistent/qindaqt-absent-session-bus"));
    const ProbeOutcome outcome = runProbeChild(environment);
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(!outcome.applied);
    QVERIFY(outcome.elapsed >= 0 && outcome.elapsed < 5'000);
}

void FontSessionBootstrapTests::probeWithoutServiceChangesNothing()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const ProbeOutcome outcome =
        runProbeChild(busChildEnvironment(bus, scratch.filePath(QStringLiteral("home")), {}));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(!outcome.applied);
    QVERIFY(outcome.family != QStringLiteral("Liberation Mono"));
}

void FontSessionBootstrapTests::probeWithWrongTypedSnapshotChangesNothing()
{
    // AGENT-NOTE: review finding P1-4 — wrong-typed confirmed values must be
    // rejected wholesale at the bootstrap, exactly as at the bridge.
    PrivateBus bus;
    QVERIFY(bus.start());
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const QString fontconfigFile = stageFixtureFontconfig(scratch);
    QVERIFY(!fontconfigFile.isEmpty());

    ServiceChild service;
    QVERIFY(service.start(busChildEnvironment(bus, scratch.filePath(QStringLiteral("service-home")),
                                              {}),
                          {QString::fromLatin1(FakeServiceModePrefix) + QStringLiteral("wrong-types")},
                          bus.connection()));

    const ProbeOutcome outcome =
        runProbeChild(busChildEnvironment(bus, scratch.filePath(QStringLiteral("probe-home")),
                                          fontconfigFile));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(!outcome.applied);
    QVERIFY(outcome.family != QStringLiteral("Liberation Mono"));
}

void FontSessionBootstrapTests::probeWithMalformedEnvelopeChangesNothing()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const QString fontconfigFile = stageFixtureFontconfig(scratch);
    QVERIFY(!fontconfigFile.isEmpty());

    ServiceChild service;
    QVERIFY(service.start(busChildEnvironment(bus, scratch.filePath(QStringLiteral("service-home")),
                                              {}),
                          {QString::fromLatin1(FakeServiceModePrefix) + QStringLiteral("bad-envelope")},
                          bus.connection()));

    const ProbeOutcome outcome =
        runProbeChild(busChildEnvironment(bus, scratch.filePath(QStringLiteral("probe-home")),
                                          fontconfigFile));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(!outcome.applied);
    QVERIFY(outcome.family != QStringLiteral("Liberation Mono"));
}

void FontSessionBootstrapTests::probeWithUnresolvedFamilyChangesNothing()
{
    // AGENT-CONTRACT: the confirmed family must resolve in the live catalog
    // produced by the productionDefault() discovery; an unresolvable family
    // is fail-closed.
    PrivateBus bus;
    QVERIFY(bus.start());
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const QString fontconfigFile = stageFixtureFontconfig(scratch);
    QVERIFY(!fontconfigFile.isEmpty());

    ServiceChild service;
    QVERIFY(service.start(busChildEnvironment(bus, scratch.filePath(QStringLiteral("service-home")),
                                              {}),
                          {QString::fromLatin1(FakeServiceModePrefix)
                           + QStringLiteral("unresolved-family")},
                          bus.connection()));

    const ProbeOutcome outcome =
        runProbeChild(busChildEnvironment(bus, scratch.filePath(QStringLiteral("probe-home")),
                                          fontconfigFile));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(!outcome.applied);
    QVERIFY(outcome.family != QStringLiteral("QindaQt Missing Family XYZ"));
}

void FontSessionBootstrapTests::probeAppliesConfirmedPreferencesFromFakeService()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const QString fontconfigFile = stageFixtureFontconfig(scratch);
    QVERIFY(!fontconfigFile.isEmpty());

    ServiceChild service;
    QVERIFY(service.start(busChildEnvironment(bus, scratch.filePath(QStringLiteral("service-home")),
                                              {}),
                          {QString::fromLatin1(FakeServiceModePrefix) + QStringLiteral("valid")},
                          bus.connection()));

    const ProbeOutcome outcome =
        runProbeChild(busChildEnvironment(bus, scratch.filePath(QStringLiteral("probe-home")),
                                          fontconfigFile));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    // The production composition read the confirmed snapshot, resolved the
    // family through productionDefault() discovery, and applied it BEFORE the
    // probe constructed its QGuiApplication.
    QVERIFY(outcome.applied);
    QCOMPARE(outcome.family, QStringLiteral("Liberation Mono"));
    QCOMPARE(outcome.pointSize, 13.5);
    QCOMPARE(outcome.hinting, static_cast<int>(QFont::PreferFullHinting));
    QCOMPARE(outcome.noAntialias, 0);
}

void FontSessionBootstrapTests::probeAppliesConfirmedPreferencesFromRealService()
{
    // AGENT-NOTE: review finding P1-1 — the end-to-end production composition
    // row: the real qindaqt-settings-service on a private bus, confirmed
    // fonts.* committed through the real CommitUserTransaction, then the
    // pre-application probe. The unrepaired candidate had no production
    // composition at all (the provider was never invoked by shipped code).
    PrivateBus bus;
    QVERIFY(bus.start());
    QTemporaryDir scratch;
    QVERIFY(scratch.isValid());
    const QString fontconfigFile = stageFixtureFontconfig(scratch);
    QVERIFY(!fontconfigFile.isEmpty());
    QVERIFY(QDir().mkpath(scratch.filePath(QStringLiteral("config"))));
    QVERIFY(QDir().mkpath(scratch.filePath(QStringLiteral("data"))));

    QProcessEnvironment serviceEnvironment =
        busChildEnvironment(bus, scratch.filePath(QStringLiteral("service-home")), {});
    serviceEnvironment.insert(QStringLiteral("XDG_CONFIG_HOME"),
                              scratch.filePath(QStringLiteral("config")));
    serviceEnvironment.insert(QStringLiteral("XDG_DATA_HOME"),
                              scratch.filePath(QStringLiteral("data")));
    serviceEnvironment.insert(QStringLiteral("QINDAQT_SETTINGS_SCHEMA_DIR"),
                              QStringLiteral(QINDAQT_TEST_SETTINGS_SCHEMA_DIR));

    QProcess service;
    service.setProcessEnvironment(serviceEnvironment);
    service.setProgram(QStringLiteral(QINDAQT_TEST_SETTINGS_SERVICE_EXECUTABLE));
    service.start();
    QVERIFY(service.waitForStarted(5'000));
    const auto stopService = qScopeGuard([&service] {
        service.terminate();
        if (!service.waitForFinished(2'000)) {
            service.kill();
            service.waitForFinished(2'000);
        }
    });
    QVERIFY(waitForServiceOwner(bus.connection(), 10'000));

    // Read the baseline, then commit confirmed fonts.* through the real wire.
    const QDBusConnection &connection = bus.connection();
    QDBusMessage snapshotCall = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::GetSnapshotMethod));
    snapshotCall << QStringList{QStringLiteral("fonts.family")};
    const QDBusReply<QVariantMap> baseline(connection.call(snapshotCall, QDBus::Block, 5'000));
    QVERIFY2(baseline.isValid(), qPrintable(baseline.error().message()));
    const QVariantMap baselineWire = baseline.value();
    const QString epoch = baselineWire.value(QLatin1StringView(WireContract::FieldEpoch)).toString();
    const quint64 revision =
        baselineWire.value(QLatin1StringView(WireContract::FieldRevision)).toULongLong();
    QVERIFY(!epoch.isEmpty());

    QVariantList operations;
    const QList<QPair<QString, QVariant>> writes = {
        {QStringLiteral("fonts.family"), QStringLiteral("Liberation Mono")},
        {QStringLiteral("fonts.pointSize"), 13.5},
        {QStringLiteral("fonts.hinting"), QStringLiteral("full")},
    };
    for (const auto &[key, value] : writes) {
        operations.append(QVariantMap{
            {QLatin1StringView(WireContract::FieldKey), key},
            {QLatin1StringView(WireContract::FieldKind),
             QString::fromLatin1(WireContract::OperationKindSet)},
            {QLatin1StringView(WireContract::FieldValue), value}});
    }
    QDBusMessage commitCall = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName),
        QString::fromLatin1(WireContract::CommitUserTransactionMethod));
    commitCall << epoch << revision << operations;
    const QDBusReply<QVariantMap> commitReply(connection.call(commitCall, QDBus::Block, 5'000));
    QVERIFY2(commitReply.isValid(), qPrintable(commitReply.error().message()));
    const QVariantMap commitWire = commitReply.value();
    QCOMPARE(commitWire.value(QLatin1StringView(WireContract::FieldStatus)).toUInt(),
             quint32(SettingsWireStatus::Applied));

    const ProbeOutcome outcome =
        runProbeChild(busChildEnvironment(bus, scratch.filePath(QStringLiteral("probe-home")),
                                          fontconfigFile));
    QVERIFY2(outcome.completed, qPrintable(outcome.raw));
    QVERIFY(outcome.applied);
    QCOMPARE(outcome.family, QStringLiteral("Liberation Mono"));
    QCOMPARE(outcome.pointSize, 13.5);
    QCOMPARE(outcome.hinting, static_cast<int>(QFont::PreferFullHinting));
}

int main(int argc, char **argv)
{
    const QStringList args = QStringList(argv + 1, argv + argc);
    for (const QString &arg : args) {
        if (arg == QLatin1String(ProbeMode)) {
            return runProbe(argc, argv);
        }
        if (arg.startsWith(QLatin1String(FakeServiceModePrefix))) {
            return runFakeService(argc, argv,
                                  arg.mid(QString::fromLatin1(FakeServiceModePrefix).size()));
        }
    }
    QCoreApplication application(argc, argv);
    FontSessionBootstrapTests tests;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_font_session_bootstrap.moc"
