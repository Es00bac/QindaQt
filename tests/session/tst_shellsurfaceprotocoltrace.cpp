// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellsurfaceprotocoltrace.h"

#include <QTest>

#include <algorithm>

using namespace QindaQt::Test;

namespace {

QByteArray request(const QByteArray &message)
{
    return "[1.000] {Default Queue}  -> " + message + '\n';
}

QByteArray event(const QByteArray &message)
{
    return "[1.100] {Default Queue} " + message + '\n';
}

QByteArray roleRequest(int initialLayer = 1, QByteArray roleId = "40",
                       QByteArray surfaceId = "35")
{
    return request("zwlr_layer_shell_v1#39.get_layer_surface(new id "
                   "zwlr_layer_surface_v1#" + roleId + ", wl_surface#" + surfaceId +
                   ", wl_output#23, " + QByteArray::number(initialLayer) + ", \"dock\")");
}

QByteArray topStateRequests(QByteArray roleId = "40", int layer = 2)
{
    return request("zwlr_layer_surface_v1#" + roleId +
                   ".set_layer(" + QByteArray::number(layer) + ")") +
        request("zwlr_layer_surface_v1#" + roleId + ".set_anchor(13)") +
        request("zwlr_layer_surface_v1#" + roleId + ".set_exclusive_zone(30)") +
        request("zwlr_layer_surface_v1#" + roleId + ".set_exclusive_edge(1)") +
        request("zwlr_layer_surface_v1#" + roleId + ".set_size(0, 30)");
}

QByteArray commit(QByteArray surfaceId = "35")
{
    return request("wl_surface#" + surfaceId + ".commit()");
}

QByteArray configure(QByteArray serial, int width = 1920, QByteArray roleId = "40")
{
    return event("zwlr_layer_surface_v1#" + roleId + ".configure(" + serial + ", " +
                 QByteArray::number(width) + ", 30)");
}

QByteArray acknowledge(QByteArray serial, QByteArray roleId = "40")
{
    return request("zwlr_layer_surface_v1#" + roleId + ".ack_configure(" + serial + ")");
}

QByteArray attach(QByteArray bufferId = "46", QByteArray surfaceId = "35")
{
    return request("wl_surface#" + surfaceId + ".attach(wl_buffer#" + bufferId + ", 0, 0)");
}

QByteArray validMappedTrace()
{
    return roleRequest() + topStateRequests() + commit() + configure("8") +
        acknowledge("8") + attach() + commit();
}

const LayerSurfaceProtocolEvidence &onlySurface(const ShellSurfaceProtocolTrace &trace)
{
    const auto &surfaces = trace.evidence().surfacesByRoleId;
    Q_ASSERT(surfaces.size() == 1);
    return surfaces.cbegin().value();
}

} // namespace

class ShellSurfaceProtocolTraceTest final : public QObject {
    Q_OBJECT

private slots:
    void fragmentedMappedEpoch();
    void rejectsOverlongInputBeforeParsing();
    void distinguishesPendingAndCommittedLayer();
    void selectsOnlyAcknowledgedMappedConfigure();
    void rejectsInvalidConfigureOrderingAndDuplicates();
    void rejectsIdentityReuseAndDestroyedTraffic();
    void acknowledgeWithoutNonNullMappingIsIncomplete();
};

void ShellSurfaceProtocolTraceTest::fragmentedMappedEpoch()
{
    ShellSurfaceProtocolTrace trace;
    const QByteArray input = validMappedTrace();
    qsizetype offset = 0;
    constexpr qsizetype fragmentSizes[] = {1, 2, 5, 3, 17, 4, 29};
    qsizetype fragment = 0;
    while (offset < input.size()) {
        const qsizetype count = std::min(fragmentSizes[fragment % 7], input.size() - offset);
        trace.ingest(input.mid(offset, count));
        offset += count;
        ++fragment;
    }
    trace.finish();

    QVERIFY(trace.evidence().isUsable());
    QVERIFY(trace.evidence().provesMappedSurfaces(1));
    const auto &surface = onlySurface(trace);
    QCOMPARE(surface.committedEpoch, 2);
    QCOMPARE(surface.committedState.layer, std::optional<int>(2));
    QVERIFY(surface.activeBufferMapping.has_value());
    QCOMPARE(surface.activeBufferMapping->configureSerial, QStringLiteral("8"));
    QCOMPARE(surface.activeBufferMapping->configureCommittedEpoch, 1);
    QCOMPARE(surface.activeBufferMapping->commitEpoch, 2);
    QVERIFY(surface.activeBufferMapping->attachOrder >
            *surface.configurationsBySerial.value(QStringLiteral("8")).acknowledgeOrder);
    QVERIFY(surface.activeBufferMapping->commitOrder >
            surface.activeBufferMapping->attachOrder);
}

void ShellSurfaceProtocolTraceTest::rejectsOverlongInputBeforeParsing()
{
    ShellSurfaceProtocolTrace newlineTrace;
    newlineTrace.ingest(QByteArray(70 * 1024, 'x') + '\n');
    QVERIFY(newlineTrace.evidence().inputTruncated);
    QVERIFY(newlineTrace.evidence().surfacesByRoleId.isEmpty());

    ShellSurfaceProtocolTrace chunkTrace;
    chunkTrace.ingest(QByteArray(300 * 1024, 'x'));
    QVERIFY(chunkTrace.evidence().inputTruncated);

    ShellSurfaceProtocolTrace fragmentedLine;
    fragmentedLine.ingest(QByteArray(40 * 1024, 'x'));
    fragmentedLine.ingest(QByteArray(30 * 1024, 'x'));
    QVERIFY(fragmentedLine.evidence().inputTruncated);

    ShellSurfaceProtocolTrace captureTrace;
    captureTrace.ingest(request(
        "zwlr_layer_shell_v1#39.get_layer_surface(new id zwlr_layer_surface_v1#40, "
        "wl_surface#35, wl_output#23, 2, \"" + QByteArray(129, 'd') + "\")"));
    QVERIFY(captureTrace.evidence().surfacesByRoleId.isEmpty());
    QVERIFY(!captureTrace.evidence().inputTruncated);
    QVERIFY(captureTrace.evidence().protocolAmbiguous);

    ShellSurfaceProtocolTrace numericCaptureTrace;
    numericCaptureTrace.ingest(validMappedTrace());
    numericCaptureTrace.ingest(
        request("zwlr_layer_surface_v1#40.set_layer(12345678901)"));
    QVERIFY(numericCaptureTrace.evidence().protocolAmbiguous);
    QVERIFY(!numericCaptureTrace.evidence().provesMappedSurfaces(1));
}

void ShellSurfaceProtocolTraceTest::distinguishesPendingAndCommittedLayer()
{
    ShellSurfaceProtocolTrace committedChange;
    committedChange.ingest(validMappedTrace());
    committedChange.ingest(request("zwlr_layer_surface_v1#40.set_layer(3)"));
    committedChange.ingest(commit());
    QCOMPARE(onlySurface(committedChange).committedState.layer, std::optional<int>(3));

    ShellSurfaceProtocolTrace uncommittedChange;
    uncommittedChange.ingest(validMappedTrace());
    uncommittedChange.ingest(request("zwlr_layer_surface_v1#40.set_layer(3)"));
    const auto &surface = onlySurface(uncommittedChange);
    QCOMPARE(surface.committedState.layer, std::optional<int>(2));
    QCOMPARE(surface.pendingState.layer, std::optional<int>(3));
    QCOMPARE(surface.activeBufferMapping->committedState.layer, std::optional<int>(2));
}

void ShellSurfaceProtocolTraceTest::selectsOnlyAcknowledgedMappedConfigure()
{
    ShellSurfaceProtocolTrace trace;
    trace.ingest(roleRequest() + topStateRequests() + commit());
    trace.ingest(configure("7", 640));
    trace.ingest(configure("8", 1920));
    trace.ingest(acknowledge("8") + attach() + commit());

    QVERIFY(trace.evidence().isUsable());
    QVERIFY(trace.evidence().provesMappedSurfaces(1));
    const auto &surface = onlySurface(trace);
    QCOMPARE(surface.configurationsBySerial.size(), 2);
    QVERIFY(!surface.configurationsBySerial.value(QStringLiteral("7"))
                 .acknowledgeOrder.has_value());
    QCOMPARE(surface.activeBufferMapping->configureSerial, QStringLiteral("8"));
}

void ShellSurfaceProtocolTraceTest::rejectsInvalidConfigureOrderingAndDuplicates()
{
    ShellSurfaceProtocolTrace acknowledgeFirst;
    acknowledgeFirst.ingest(roleRequest() + topStateRequests() + commit() +
                            acknowledge("8"));
    QVERIFY(acknowledgeFirst.evidence().protocolAmbiguous);
    QVERIFY(!acknowledgeFirst.evidence().provesMappedSurfaces(1));

    ShellSurfaceProtocolTrace duplicateConfigure;
    duplicateConfigure.ingest(roleRequest() + topStateRequests() + commit() +
                              configure("8") + configure("8"));
    QVERIFY(duplicateConfigure.evidence().protocolAmbiguous);

    ShellSurfaceProtocolTrace configureBeforeCommit;
    configureBeforeCommit.ingest(roleRequest() + topStateRequests() + configure("8"));
    QVERIFY(configureBeforeCommit.evidence().protocolAmbiguous);
}

void ShellSurfaceProtocolTraceTest::rejectsIdentityReuseAndDestroyedTraffic()
{
    ShellSurfaceProtocolTrace roleReuse;
    roleReuse.ingest(roleRequest());
    roleReuse.ingest(request("zwlr_layer_surface_v1#40.destroy()"));
    roleReuse.ingest(roleRequest());
    QVERIFY(roleReuse.evidence().identityAmbiguous);

    ShellSurfaceProtocolTrace backingReuse;
    backingReuse.ingest(roleRequest());
    backingReuse.ingest(roleRequest(2, "41", "35"));
    QVERIFY(backingReuse.evidence().identityAmbiguous);

    ShellSurfaceProtocolTrace destroyedTraffic;
    destroyedTraffic.ingest(roleRequest());
    destroyedTraffic.ingest(request("zwlr_layer_surface_v1#40.destroy()"));
    destroyedTraffic.ingest(request("zwlr_layer_surface_v1#40.set_anchor(13)"));
    QVERIFY(destroyedTraffic.evidence().protocolAmbiguous);

    ShellSurfaceProtocolTrace preRoleCommit;
    preRoleCommit.ingest(commit());
    preRoleCommit.ingest(roleRequest());
    QVERIFY(preRoleCommit.evidence().identityAmbiguous);
}

void ShellSurfaceProtocolTraceTest::acknowledgeWithoutNonNullMappingIsIncomplete()
{
    ShellSurfaceProtocolTrace acknowledgeOnly;
    acknowledgeOnly.ingest(roleRequest() + topStateRequests() + commit() +
                           configure("8") + acknowledge("8"));
    QVERIFY(!onlySurface(acknowledgeOnly).mapped);
    QVERIFY(!acknowledgeOnly.evidence().provesMappedSurfaces(1));

    ShellSurfaceProtocolTrace nullAttach;
    nullAttach.ingest(roleRequest() + topStateRequests() + commit() +
                      configure("8") + acknowledge("8") +
                      request("wl_surface#35.attach(nil, 0, 0)") + commit());
    QVERIFY(!onlySurface(nullAttach).mapped);
    QVERIFY(!nullAttach.evidence().provesMappedSurfaces(1));

    ShellSurfaceProtocolTrace attachWithoutAcknowledge;
    attachWithoutAcknowledge.ingest(roleRequest() + topStateRequests() + commit() +
                                    configure("8") + attach() + commit());
    QVERIFY(attachWithoutAcknowledge.evidence().protocolAmbiguous);
    QVERIFY(!attachWithoutAcknowledge.evidence().provesMappedSurfaces(1));

    ShellSurfaceProtocolTrace acknowledgeAfterAttach;
    acknowledgeAfterAttach.ingest(roleRequest() + topStateRequests() + commit() +
                                  configure("8") + attach() + acknowledge("8") +
                                  commit());
    QVERIFY(acknowledgeAfterAttach.evidence().protocolAmbiguous);
    QVERIFY(!acknowledgeAfterAttach.evidence().provesMappedSurfaces(1));
}

QTEST_APPLESS_MAIN(ShellSurfaceProtocolTraceTest)

#include "tst_shellsurfaceprotocoltrace.moc"
