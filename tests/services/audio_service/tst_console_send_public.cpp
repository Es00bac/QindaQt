// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/fake_audio_backend.h"
#include "support/console_send_fixture.h"
#include "console_endpoints_p.h"
#include "wireplumber_routing_p.h"
#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/qt_audio_transport.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <qindaqt/services/audio_service/resident_audio_service.h>
#include <QtCore/QProcess>
#include <QtCore/QScopeGuard>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUuid>
#include <QtTest>
#include <algorithm>
#include <cmath>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;
namespace {
class PrivateBus final
{
public:
    bool start()
    {
        process.start(QStringLiteral("dbus-daemon"), {QStringLiteral("--session"),
            QStringLiteral("--nofork"), QStringLiteral("--nopidfile"), QStringLiteral("--print-address=1")});
        if (!process.waitForStarted(2000) || !process.waitForReadyRead(2000)) return false;
        const QString address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-send-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        client = QDBusConnection::connectToBus(address, name);
        service = QDBusConnection::connectToBus(address, name + QStringLiteral("-service"));
        return client.isConnected() && service.isConnected();
    }
    ~PrivateBus()
    {
        QDBusConnection::disconnectFromBus(name);
        QDBusConnection::disconnectFromBus(name + QStringLiteral("-service"));
        process.terminate();
        if (!process.waitForFinished(1000)) { process.kill(); process.waitForFinished(2000); }
    }
    QProcess process;
    QString name;
    QDBusConnection client{QStringLiteral("invalid-client")};
    QDBusConnection service{QStringLiteral("invalid-service")};
};

Snapshot applicationGraph()
{
    Snapshot graph = audioSnapshot();
    graph.outputs.removeLast();
    Device second = graph.outputs.first();
    second.handle.serial = 12;
    second.name = QStringLiteral("Headphones");
    second.nodeName = QStringLiteral("fixture.headphones");
    second.isDefault = false;
    graph.outputs.append(second);
    for (int index = 0; index < 3; ++index) {
        Device input = second;
        input.handle.serial = static_cast<quint64>(40 + index);
        input.nodeName = ConsoleEndpoints::stripSinkNodeName(QStringLiteral("strip.virtual.%1").arg(index + 1));
        input.name = QStringLiteral("App input %1").arg(index + 1);
        input.virtualDevice = true;
        graph.outputs.append(input);
    }
    const Stream original = graph.streams.first();
    graph.streams.clear();
    const QStringList names{QStringLiteral("Native player"), QStringLiteral("Android app"), QStringLiteral("Windows app")};
    for (int index = 0; index < 3; ++index) {
        Stream stream = original;
        stream.handle.serial = static_cast<quint64>(60 + index);
        stream.applicationName = names.at(index);
        graph.streams.append(stream);
    }
    return graph;
}
}

class ConsoleSendPublicTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void threeApplicationsKeepIndependentSendsToTwoConfiguredOutputs();
};

void ConsoleSendPublicTests::threeApplicationsKeepIndependentSendsToTwoConfiguredOutputs()
{
    // Every persisted input is private. The real Audio1 server/client/codec
    // runs on a private bus; graph identities and setter acceptance are injected.
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QList<QByteArray> keys{"QINDAQT_AUDIO_PRESET_DIR", "QINDAQT_AUDIO_MACRO_PATH",
        "QINDAQT_AUDIO_VBAN_PATH", "QINDAQT_AUDIO_LATENCY_PATH"};
    QMap<QByteArray, std::optional<QByteArray>> saved;
    for (const auto &key : keys) {
        saved[key] = qEnvironmentVariableIsSet(key.constData())
            ? std::optional<QByteArray>(qgetenv(key.constData())) : std::nullopt;
        qputenv(key.constData(), files.filePath(QString::fromLatin1(key)).toUtf8());
    }
    const auto restore = qScopeGuard([&] {
        for (const auto &key : keys) {
            if (saved[key]) qputenv(key.constData(), *saved[key]);
            else qunsetenv(key.constData());
        }
    });
    PrivateBus bus;
    QVERIFY(bus.start());
    auto ownedBackend = std::make_unique<FakeAudioBackend>();
    auto *backend = ownedBackend.get();
    const QString serviceName = QStringLiteral("org.qindaqt.AudioSendTest");
    ResidentAudioService service(std::move(ownedBackend), bus.service, serviceName, nullptr,
        files.filePath(QStringLiteral("console.json")));
    QCOMPARE(service.start(), ServiceStartStatus::Started);
    Snapshot graph = applicationGraph();
    backend->publish(graph);
    QtAudioTransport transport(bus.client, serviceName);
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 2000);

    for (int app = 0; app < 3; ++app) {
        const qsizetype before = completed.size();
        const quint64 request = client.moveStream({7, static_cast<quint64>(60 + app)},
                                                  {7, static_cast<quint64>(40 + app)});
        QVERIFY(request != 0);
        QTRY_COMPARE_WITH_TIMEOUT(backend->operations.size(), app + 1, 2000);
        graph.streams[app].target = {7, static_cast<quint64>(40 + app)};
        ++graph.revision;
        backend->publish(graph);
        backend->finish(backend->operations.last().operationId,
            {.status = BackendOperationStatus::Succeeded, .reasonCode = QStringLiteral("ok"), .diagnostic = {}});
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), before + 1, 2000);
        QCOMPARE(completed.last().at(1).value<OperationResult>().status, OperationStatus::Succeeded);
        QTRY_COMPARE_WITH_TIMEOUT(client.snapshot().streams.at(app).target, graph.streams.at(app).target, 2000);
    }
    const auto apply = [&](const std::function<quint64()> &operation) {
        const qsizetype before = completed.size();
        const quint64 initiatingRevision = client.snapshot().revision;
        const quint64 request = operation();
        if (request == 0 || !QTest::qWaitFor([&] { return completed.size() == before + 1; }, 2000)) return false;
        const auto result = completed.last().at(1).value<OperationResult>();
        if (result.status != OperationStatus::Succeeded) {
            qWarning() << "Public console operation failed:" << result.reasonCode;
            return false;
        }
        return completed.last().at(0).toULongLong() == request
            && (!operationTargetsHandle(result.kind) || result.initiatingRevision == initiatingRevision)
            // Every next operation uses the public readback from this change.
            // Completion may precede the Changed-triggered snapshot reply.
            && QTest::qWaitFor([&] {
                return client.snapshot().epoch == result.observedEpoch
                    && client.snapshot().revision >= result.observedRevision;
            }, 2000);
    };
    QVERIFY(apply([&] { return client.setBusTarget(QStringLiteral("bus.a1"), {7, 10}); }));
    QVERIFY(apply([&] { return client.setBusTarget(QStringLiteral("bus.a2"), {7, 12}); }));
    QVERIFY(apply([&] { return client.setBusGain(QStringLiteral("bus.a2"), -6.0); }));
    for (int app = 1; app <= 3; ++app) {
        const QString strip = QStringLiteral("strip.virtual.%1").arg(app);
        QVERIFY(apply([&] { return client.setStripGain(strip, -3.0 * app); }));
        QVERIFY(apply([&] { return client.setStripSend(strip, app == 3 ? 1U : 0U, true, -6.0); }));
    }
    QVERIFY(apply([&] { return client.setStripSend(QStringLiteral("strip.virtual.1"), 1, true, -12.0); }));
    QTRY_COMPARE_WITH_TIMEOUT(client.snapshot().console.strips.at(5).sends.at(1).gainDb, -12.0, 2000);
    const Snapshot readback = client.snapshot();
    QCOMPARE(readback.streams.size(), 3);
    QCOMPARE(readback.console.buses.at(0).targetSerial, quint64(10));
    QCOMPARE(readback.console.buses.at(1).targetSerial, quint64(12));
    QCOMPARE(readback.console.buses.at(1).gainDb, -6.0);
    for (int app = 0; app < 3; ++app) {
        QCOMPARE(readback.console.strips.at(app + 5).gainDb, -3.0 * (app + 1));
        QCOMPARE(readback.streams.at(app).applicationName, graph.streams.at(app).applicationName);
    }
    QCOMPARE(backend->routing.size(), 4);
    ConsoleSendFixture fixture;
    fixture.resolve = [&](const BackendRoutingEdge &route) {
        const auto named = [&](const Handle handle) {
            for (const auto &device : graph.outputs) if (device.handle == handle) return device.nodeName;
            return QString{};
        };
        return SendEndpoints{named(route.source), named(route.target), route.sourceIsSink};
    };
    ConsoleSendReconciler controller;
    controller.reconcile(backend->routing, fixture.runtime());
    QCOMPARE(controller.size(), qsizetype(4));
    QCOMPARE(fixture.writes.size(), 4);
    for (const auto &route : backend->routing) {
        const QString name = routingNodeName(route.stripId, route.busId);
        const auto write = std::find_if(fixture.writes.cbegin(), fixture.writes.cend(), [&](const auto &value) {
            return value.node.name == name;
        });
        QVERIFY(write != fixture.writes.cend());
        const double expected = std::pow(10.0, (route.stripGainDb + route.gainDb + route.busGainDb) / 20.0);
        QVERIFY(std::abs(write->amplitude.left - expected) < 1e-10);
        const auto module = std::find_if(fixture.modules.cbegin(), fixture.modules.cend(), [&](const auto &value) {
            return value->key == name;
        });
        QVERIFY(module != fixture.modules.cend());
        const QString target = route.target.serial == 10 ? graph.outputs.at(0).nodeName : graph.outputs.at(1).nodeName;
        QVERIFY((*module)->arguments.contains(QStringLiteral("target.object = \"%1\"").arg(target).toUtf8()));
    }
    client.stop();
    service.stop();
}
QTEST_GUILESS_MAIN(ConsoleSendPublicTests)
#include "tst_console_send_public.moc"
