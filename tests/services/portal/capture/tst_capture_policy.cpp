// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/capture_types.h>
#include <QDBusMetaType>
#include <QJsonArray>
#include <QUrl>
#include <QtTest>
#include <limits>
using namespace QindaQt::Services::Portal;
class CapturePolicyTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void exactWireAndSelectionSurface() {
        registerCaptureWireTypes();
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<CaptureColor>())), QByteArray("(ddd)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<CaptureCoordinate>())), QByteArray("(ii)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<CaptureStreams>())), QByteArray("a(ua{sv})"));
        QVERIFY(validScreenCastSelection({})); QVERIFY(validScreenCastSelection({{"types", 1U}, {"multiple", false}, {"cursor_mode", 1U}}));
        QVERIFY(validScreenCastSelection({{"multiple", true}, {"cursor_mode", 2U}}));
        QVERIFY(validScreenCastSelection({{"multiple", true}, {"cursor_mode", 4U}}));
        for (const auto &options : {QVariantMap{{"types", 2U}}, QVariantMap{{"multiple", 1}}, QVariantMap{{"cursor_mode", 3U}}, QVariantMap{{"cursor_mode", 0U}}, QVariantMap{{"cursor_mode", 2}}, QVariantMap{{"types", 1}}, QVariantMap{{"persist_mode", 3U}}, QVariantMap{{"persist_mode", 1}}, QVariantMap{{"restore_data", "fake"}}, QVariantMap{{"window", 1U}}}) QVERIFY(!validScreenCastSelection(options));
        // The frontend always forwards persist_mode for callers such as OBS.
        QVERIFY(validScreenCastSelection({{"persist_mode", 2U}, {"cursor_mode", 2U}}));
        QCOMPARE(captureCaller("/org/freedesktop/portal/desktop/request/1_28/test"), QString(":1.28"));
        QVERIFY(captureCaller("/org/freedesktop/portal/desktop/request/stranger/test").isEmpty());
    }
    void streamOptionsSurviveOwnedHelperTransport() {
        CaptureRequest request; request.kind = CaptureKind::Stream; request.session = "/session/owned";
        request.multiple = true; request.cursorMode = 4;
        const auto frame = captureFrame(request, "/private/capture", ":1.2");
        const auto decoded = captureRequestFromFrame(frame); QVERIFY(decoded);
        QVERIFY(decoded->multiple); QCOMPARE(decoded->cursorMode, 4U);
        auto bad = frame; bad.insert("cursor_mode", 3); QVERIFY(!captureRequestFromFrame(bad));
        bad = frame; bad.insert("multiple", 1); QVERIFY(!captureRequestFromFrame(bad));
        bad = frame; bad.insert("kind", 0); QVERIFY(!captureRequestFromFrame(bad));
        auto legacy = frame; legacy.remove("multiple"); legacy.remove("cursor_mode");
        const auto old = captureRequestFromFrame(legacy); QVERIFY(old); QVERIFY(!old->multiple); QCOMPARE(old->cursorMode, 1U);
    }
    void batchPublicationIsAtomicAndRejectsDuplicateNodes() {
        QJsonObject first{{"node", 41}, {"x", 0}, {"y", 0}, {"width", 1920}, {"height", 1080}, {"name", "First"}};
        QJsonObject second = first; second.insert("node", 42); second.insert("x", 1920); second.insert("name", "Second");
        const auto batch = captureResults(CaptureKind::Stream, {{"streams", QJsonArray{first, second}}}, {});
        QVERIFY(batch); QVERIFY(validCapturePublication(CaptureKind::Stream, *batch));
        QCOMPARE(batch->value("streams").value<CaptureStreams>().size(), 2);
        QVERIFY(!captureResults(CaptureKind::Stream, {{"streams", QJsonArray{first, first}}}, {}));
        second.insert("width", 0);
        QVERIFY(!captureResults(CaptureKind::Stream, {{"streams", QJsonArray{first, second}}}, {}));
        QVERIFY(!captureResults(CaptureKind::Stream, {{"streams", QJsonArray{}}}, {}));
        QVERIFY(!captureResults(CaptureKind::Stream, {{"streams", QJsonArray{QJsonObject{{"streams", QJsonArray{first}}}}}}, {}));
        QJsonArray excessive; for (int i = 0; i != 17; ++i) { first.insert("node", i + 1); excessive.append(first); }
        QVERIFY(!captureResults(CaptureKind::Stream, {{"streams", excessive}}, {}));
    }
    void screenshotBoundsParentAndFrames() {
        auto request = screenshotRequest("org.test.Caller", "wayland:opaque literal", {{"interactive", true}, {"permission_store_checked", true}}, false); QVERIFY(request);
        QCOMPARE(request->kind, CaptureKind::Screenshot); QVERIFY(request->interactive);
        QVERIFY(!screenshotRequest(QString(256, 'x'), {}, {}, false));
        QVERIFY(!screenshotRequest({}, "x11:1", {}, false));
        QVERIFY(!screenshotRequest({}, {}, {{"modal", "true"}}, false));
        QVERIFY(!screenshotRequest({}, {}, {{"fd", 4}}, false));
        const auto frame = captureFrame(*request, "/private/literal %$`", ":1.2"); const auto roundtrip = captureRequestFromFrame(frame); QVERIFY(roundtrip); QCOMPARE(roundtrip->parent, request->parent);
        auto malformed = frame; malformed.insert("kind", 1.5); QVERIFY(!captureRequestFromFrame(malformed));
        malformed = frame; malformed.insert("directory", "relative"); QVERIFY(!captureRequestFromFrame(malformed));
    }
    void realPublicationShapeAndNoLatePayload() {
        // AGENT-CONTRACT: FullyEncoded preserves legal '$' path sub-delimiters;
        // space, percent and backtick are encoded. The literal filename must
        // round-trip without shell expansion or a second encoding pass.
        const QString uri = "file:///private/literal%20%25$%60/screenshot.png";
        const QString filename = "/private/literal %$`/screenshot.png";
        QCOMPARE(QUrl::fromLocalFile(filename).toString(QUrl::FullyEncoded), uri);
        const auto image = captureResults(CaptureKind::Screenshot, {{"uri", uri}}, "/private/literal %$`");
        QVERIFY(image); QVERIFY(validCapturePublication(CaptureKind::Screenshot, *image));
        QCOMPARE(QUrl(image->value("uri").toString()).toLocalFile(), filename);
        QVERIFY(!captureResults(CaptureKind::Screenshot, {{"uri", "file:///private/literal%20%25%24%60/screenshot.png"}}, "/private/literal %$`"));
        QVERIFY(!captureResults(CaptureKind::Screenshot, {{"uri", "file:///other/screenshot.png"}}, "/private/literal %$`"));
        QVERIFY(!validCapturePublication(CaptureKind::Screenshot, {{"uri", "file:///private/../secret"}}));
        QVERIFY(!validCapturePublication(CaptureKind::Screenshot, {{"uri", "file://server/private/file"}}));
        const auto color = captureResults(CaptureKind::Color, {{"color", QJsonArray{.1, .2, .3}}}, {}); QVERIFY(color); QVERIFY(validCapturePublication(CaptureKind::Color, *color));
        QVERIFY(!captureResults(CaptureKind::Color, {{"color", QJsonArray{.1, -1, .3}}}, {}));
        QVERIFY(!validCapturePublication(CaptureKind::Color, {{"color", QVariant::fromValue(CaptureColor{std::numeric_limits<double>::quiet_NaN(), 0, 0})}}));
        const auto stream = captureResults(CaptureKind::Stream, {{"node", 42}, {"x", -100}, {"y", 0}, {"width", 1920}, {"height", 1080}, {"name", "Virtual screen"}}, {}); QVERIFY(stream); QVERIFY(validCapturePublication(CaptureKind::Stream, *stream));
        QVERIFY(!validCapturePublication(CaptureKind::Stream, {{"streams", QVariant::fromValue(CaptureStreams{})}}));
        QVERIFY(!validCapturePublication(CaptureKind::Stream, {{"streams", QVariant::fromValue(CaptureStreams{{0, {}}})}}));
    }
};
QTEST_GUILESS_MAIN(CapturePolicyTest)
#include "tst_capture_policy.moc"
