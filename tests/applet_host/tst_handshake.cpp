// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_handshake.h"
#include "qindaqt/applet_host/host_protocol_codec.h"

#include "qindaqt/applets/manifest_loader.h"

#include <QTest>

using namespace QindaQt::AppletHost;
using namespace QindaQt::Applets;

namespace {

AppletManifest hostedManifest()
{
    const ManifestLoadResult result = ManifestLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets/task-list.json"));
    if (!result.ok) {
        qFatal("Cannot load test manifest: %s", qPrintable(result.error));
    }
    AppletManifest manifest = result.manifest;
    manifest.entryPoint = {EntryPointKind::Qml, QStringLiteral("ui/Main.qml")};
    return manifest;
}

CapabilityEvaluation capabilityEvaluation(const AppletManifest &manifest)
{
    const CapabilityPolicyLoadResult loaded = CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    if (!loaded.ok) {
        qFatal("Cannot load test policy: %s", qPrintable(loaded.error));
    }
    return loaded.policy.evaluate(manifest, {manifest.id, PackageTrust::ThirdParty});
}

HostHello validHello(const AppletManifest &manifest, const QByteArray &token)
{
    return {.protocolVersion = ProtocolVersion::current(),
            .appletApiVersion = manifest.apiVersion,
            .manifestId = manifest.id,
            .launchToken = token};
}

} // namespace

class HandshakeTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsBoundIdentityAndPolicy();
    void negotiatesProtocolMinorVersion();
    void rejectsProtocolMajorMismatch();
    void rejectsAppletApiMismatch();
    void rejectsIdentityMismatch();
    void rejectsLaunchTokenMismatch();
    void rejectsIncompletePolicyDecisionSet();
    void roundTripsProtocolMessages();
    void rejectsMalformedProtocolMessages();
};

void HandshakeTest::acceptsBoundIdentityAndPolicy()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef0123456789abcdef");
    const CapabilityEvaluation decisions = capabilityEvaluation(manifest);
    const HandshakeResponse response = HostHandshake::evaluate(
        validHello(manifest, token), manifest, token, decisions);

    QVERIFY2(response.accepted(), qPrintable(response.message));
    QVERIFY(response.negotiatedProtocol.has_value());
    QVERIFY(*response.negotiatedProtocol == ProtocolVersion::current());
    QVERIFY(response.capabilityDecisions == decisions.decisions);
}

void HandshakeTest::negotiatesProtocolMinorVersion()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef");
    HostHello hello = validHello(manifest, token);
    hello.protocolVersion = {1, 5};
    const HandshakeResponse response = HostHandshake::evaluate(
        hello, manifest, token, capabilityEvaluation(manifest), ProtocolVersion{1, 2});

    QVERIFY(response.accepted());
    QVERIFY(response.negotiatedProtocol.has_value());
    QVERIFY(*response.negotiatedProtocol == (ProtocolVersion{1, 2}));
}

void HandshakeTest::rejectsProtocolMajorMismatch()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef");
    HostHello hello = validHello(manifest, token);
    hello.protocolVersion = {2, 0};
    const HandshakeResponse response = HostHandshake::evaluate(
        hello, manifest, token, capabilityEvaluation(manifest));
    QVERIFY(response.status == HandshakeStatus::ProtocolMismatch);
    QVERIFY(!response.accepted());
}

void HandshakeTest::rejectsAppletApiMismatch()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef");
    HostHello hello = validHello(manifest, token);
    hello.appletApiVersion = {1, 1};
    const HandshakeResponse response = HostHandshake::evaluate(
        hello, manifest, token, capabilityEvaluation(manifest), ProtocolVersion::current(),
        ApiVersion{1, 2});
    QVERIFY(response.status == HandshakeStatus::AppletApiMismatch);
}

void HandshakeTest::rejectsIdentityMismatch()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef");
    HostHello hello = validHello(manifest, token);
    hello.manifestId = QStringLiteral("different-applet");
    const HandshakeResponse response = HostHandshake::evaluate(
        hello, manifest, token, capabilityEvaluation(manifest));
    QVERIFY(response.status == HandshakeStatus::IdentityMismatch);
}

void HandshakeTest::rejectsLaunchTokenMismatch()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef");
    HostHello hello = validHello(manifest, QByteArray("wrong-token"));
    const HandshakeResponse response = HostHandshake::evaluate(
        hello, manifest, token, capabilityEvaluation(manifest));
    QVERIFY(response.status == HandshakeStatus::LaunchTokenMismatch);
}

void HandshakeTest::rejectsIncompletePolicyDecisionSet()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("0123456789abcdef");
    CapabilityEvaluation decisions = capabilityEvaluation(manifest);
    decisions.decisions.removeLast();
    const HandshakeResponse response = HostHandshake::evaluate(
        validHello(manifest, token), manifest, token, decisions);
    QVERIFY(response.status == HandshakeStatus::PolicyUnavailable);
    QVERIFY(response.capabilityDecisions.isEmpty());
}

void HandshakeTest::roundTripsProtocolMessages()
{
    const AppletManifest manifest = hostedManifest();
    const QByteArray token("binary-token\0with-null", 22);
    const HostHello hello = validHello(manifest, token);
    const HelloDecodeResult decodedHello = HostProtocolCodec::decodeHello(
        HostProtocolCodec::encodeHello(hello));
    QVERIFY2(decodedHello.ok, qPrintable(decodedHello.error));
    QVERIFY(decodedHello.hello.protocolVersion == hello.protocolVersion);
    QVERIFY(decodedHello.hello.appletApiVersion == hello.appletApiVersion);
    QCOMPARE(decodedHello.hello.manifestId, hello.manifestId);
    QCOMPARE(decodedHello.hello.launchToken, hello.launchToken);

    const HandshakeResponse response = HostHandshake::evaluate(
        hello, manifest, token, capabilityEvaluation(manifest));
    const ResponseDecodeResult decodedResponse = HostProtocolCodec::decodeResponse(
        HostProtocolCodec::encodeResponse(response));
    QVERIFY2(decodedResponse.ok, qPrintable(decodedResponse.error));
    QVERIFY(decodedResponse.response.status == response.status);
    QVERIFY(decodedResponse.response.negotiatedProtocol == response.negotiatedProtocol);
    QVERIFY(decodedResponse.response.capabilityDecisions == response.capabilityDecisions);
}

void HandshakeTest::rejectsMalformedProtocolMessages()
{
    const HelloDecodeResult malformed = HostProtocolCodec::decodeHello(
        QByteArray(R"json({"type":"hello","launchToken":"***"})json"));
    QVERIFY(!malformed.ok);

    const QByteArray oversized(HostProtocolCodec::MaximumMessageBytes + 1, 'x');
    const HelloDecodeResult tooLarge = HostProtocolCodec::decodeHello(oversized);
    QVERIFY(!tooLarge.ok);
    QVERIFY(tooLarge.error.contains(QStringLiteral("64 KiB")));
}

QTEST_GUILESS_MAIN(HandshakeTest)
#include "tst_handshake.moc"
