// SPDX-License-Identifier: GPL-3.0-or-later
// The legacy gate includes the immutable graph TU so it executes the original
// private helper, not a copied model. Fixed builds use the extracted owner seam.
#ifdef QINDAQT_LEGACY_GRAPH_SOURCE
#include QINDAQT_LEGACY_GRAPH_SOURCE
#else
#include "wireplumber_volume_p.h"
#endif
#include "support/fake_audio_backend.h"
#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::WirePlumberGraph;
using namespace QindaQt::Tests;

class ChannelProjectionTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void preservesValidatedGraph_data();
    void preservesValidatedGraph();
    void ambiguousChannelsPreserveAggregateButRefusePartialWrite();
};

void ChannelProjectionTests::preservesValidatedGraph_data()
{
    QTest::addColumn<bool>("stream");
    QTest::addColumn<bool>("known");
    QTest::addColumn<QList<int>>("indices");
    QTest::addColumn<QVector<double>>("levels");
    QTest::addColumn<QStringList>("map");
    QTest::addColumn<QVector<double>>("expected");
    for (const bool stream : {false, true}) {
        const QByteArray prefix = stream ? "stream-" : "device-";
        auto row = [&](const char *name, bool known, QList<int> indices,
                       QVector<double> levels, QStringList map,
                       QVector<double> expected) {
            QTest::newRow((prefix + name).constData())
                << stream << known << indices << levels << map << expected;
        };
        const QStringList stereo{"FL", "FR"};
        row("contracts-old-layout", true, {0, 1, 2}, {0.1, 0.2, 0.3}, stereo, {});
        row("partial-unknown", false, {0}, {0.1}, stereo, {});
        row("equal", true, {0, 1}, {0.1, 0.2}, stereo, {0.1, 0.2});
        row("expands-known", true, {0}, {0.1}, stereo, {0.1, 0.5});
        row("sparse-known", true, {1}, {0.2}, stereo, {0.5, 0.2});
        row("sparse-unknown", false, {1}, {0.2}, stereo, {});
        row("unordered", true, {1, 0}, {0.2, 0.1}, stereo, {0.1, 0.2});
        row("no-map", false, {0, 1}, {0.1, 0.2}, {}, {0.1, 0.2});
        row("unknown", false, {}, {}, stereo, {});
    }
}

void ChannelProjectionTests::preservesValidatedGraph()
{
    QFETCH(bool, stream); QFETCH(bool, known);
    QFETCH(QList<int>, indices); QFETCH(QVector<double>, levels);
    QFETCH(QStringList, map); QFETCH(QVector<double>, expected);
    GVariantBuilder channels;
    g_variant_builder_init(&channels, G_VARIANT_TYPE_VARDICT);
    for (qsizetype index = 0; index < indices.size(); ++index) {
        GVariantBuilder entry;
        g_variant_builder_init(&entry, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&entry, "{sv}", "volume", g_variant_new_double(levels.at(index)));
        const QByteArray key = QByteArray::number(indices.at(index));
        g_variant_builder_add(&channels, "{sv}", key.constData(), g_variant_builder_end(&entry));
    }
    GVariantBuilder dictionary;
    g_variant_builder_init(&dictionary, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&dictionary, "{sv}", "channelVolumes", g_variant_builder_end(&channels));
    GVariant *value = g_variant_ref_sink(g_variant_builder_end(&dictionary));
    VolumeState volume;
    volume.volume = known ? 0.5 : 0.0;
    volume.volumeKnown = known;
    volume.channelVolumes = readChannelVolumes(value);
    g_variant_unref(value);
    const auto projected = projectedChannelVolumes(volume, map);

    Snapshot snapshot = audioSnapshot(7, 4);
    auto apply = [&](auto &item) {
        item.channelMap = map; item.channelVolumes = projected;
        item.volumeKnown = known; item.volume = volume.volume;
        item.canSetVolume = known;
    };
    if (stream) apply(snapshot.streams[0]); else apply(snapshot.outputs[0]);
    const auto validation = validateSnapshot(snapshot);
    // This assertion exposes the original whole-graph rejection independently
    // of the expected-level assertion (which also catches index compaction).
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode));
    QCOMPARE(projected, expected);
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    backend.publish(audioSnapshot());
    backend.publish(snapshot);
    const auto current = coordinator.snapshot();
    QVERIFY2(validateSnapshot(current).accepted, qPrintable(validateSnapshot(current).reasonCode));
    QCOMPARE(current.availability, Availability::Ready);
    QCOMPARE(current.outputs.size(), snapshot.outputs.size());
    QCOMPARE(current.inputs.size(), snapshot.inputs.size());
    QCOMPARE(current.streams.size(), snapshot.streams.size());
    QCOMPARE(current.defaultOutput, snapshot.defaultOutput);
    QCOMPARE(current.outputs[1], snapshot.outputs[1]);
    if (stream) QCOMPARE(current.streams[0].channelVolumes, expected);
    else QCOMPARE(current.outputs[0].channelVolumes, expected);
    coordinator.stop();
}
void ChannelProjectionTests::ambiguousChannelsPreserveAggregateButRefusePartialWrite()
{
    VolumeState volume;
    volume.volume = 0.5; volume.volumeKnown = true;
    volume.channelVolumes = {0.1, 0.2, 0.3, 0.4};
    Snapshot snapshot = audioSnapshot();
    snapshot.outputs[0].channelMap = {"MONO"};
    snapshot.outputs[0].channelVolumes = projectedChannelVolumes(volume, {"MONO"});
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start(); backend.publish(snapshot);
    QVERIFY(validateSnapshot(coordinator.snapshot()).accepted);
    QCOMPARE(coordinator.snapshot().availability, Availability::Ready);
    QCOMPARE(coordinator.snapshot().outputs.size(), snapshot.outputs.size());
    QVERIFY(coordinator.snapshot().outputs[0].channelVolumes.isEmpty());
    const auto channels = coordinator.submit({.kind = OperationKind::SetChannelVolumes,
        .primary = snapshot.outputs[0].handle, .secondary = {}, .volume = 0.0,
        .muted = false, .channelVolumes = {0.25}});
    QVERIFY(!channels.pending);
    QCOMPARE(channels.immediateResult.reasonCode, QStringLiteral("invalid-target"));
    QVERIFY(backend.operations.isEmpty());
    const auto aggregate = coordinator.submit({.kind = OperationKind::SetVolume,
        .primary = snapshot.outputs[0].handle, .secondary = {}, .volume = 0.25});
    QVERIFY(aggregate.pending);
    QCOMPARE(backend.operations.size(), 1);
    QCOMPARE(backend.operations[0].request.kind, OperationKind::SetVolume);
    coordinator.stop();
}
QTEST_GUILESS_MAIN(ChannelProjectionTests)
#include "tst_wireplumber_channel_projection.moc"
