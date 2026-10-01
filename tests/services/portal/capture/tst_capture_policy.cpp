// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/capture_types.h>
#include <QDBusMetaType>
#include <QJsonArray>
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
        for (const auto &options : {QVariantMap{{"types", 2U}}, QVariantMap{{"multiple", true}}, QVariantMap{{"cursor_mode", 2U}}, QVariantMap{{"types", 1}}, QVariantMap{{"persist_mode", 1U}}, QVariantMap{{"restore_data", "fake"}}}) QVERIFY(!validScreenCastSelection(options));
        QCOMPARE(captureCaller("/org/freedesktop/portal/desktop/request/1_28/test"), QString(":1.28"));
        QVERIFY(captureCaller("/org/freedesktop/portal/desktop/request/stranger/test").isEmpty());
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
        const auto image = captureResults(CaptureKind::Screenshot, {{"uri", "file:///private/literal%20%25%24%60/screenshot.png"}}, "/private/literal %$`"); QVERIFY(image); QVERIFY(validCapturePublication(CaptureKind::Screenshot, *image));
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
