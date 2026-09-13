// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_test_support.h"

#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QtTest>

using namespace QindaQt::Apps::SettingsCustomize;
using namespace QindaQt::Apps::SettingsCustomize::TestSupport;

namespace {

class CustomizeWallpaperPreviewTests final : public QObject {
    Q_OBJECT

private slots:
    void resolvesConfiguredWallpaperIdentityAndMode();
    void tracksAuthoritativeSnapshotChanges();
    void failsClosedToExplicitFallbacks();
};

void CustomizeWallpaperPreviewTests::resolvesConfiguredWallpaperIdentityAndMode()
{
    ModelHarness harness;
    QVERIFY(harness.establish(QStringLiteral("fixture"),
                              QStringLiteral("qindaqt:fixture-wall"),
                              QStringLiteral("tiled")));
    QCOMPARE(harness.wallpaperPreview.status(), QStringLiteral("ready"));
    QCOMPARE(harness.wallpaperPreview.source(),
             QUrl::fromLocalFile(harness.wallpaperFilePath()));
    QCOMPARE(harness.wallpaperPreview.mode(), QStringLiteral("tiled"));

    // A saved absolute path resolves the same way without the identity.
    QVERIFY(harness.updateWallpaper(harness.wallpaperFilePath(),
                                    QStringLiteral("centered"), 8));
    QTRY_COMPARE(harness.wallpaperPreview.mode(), QStringLiteral("centered"));
    QCOMPARE(harness.wallpaperPreview.status(), QStringLiteral("ready"));
    QCOMPARE(harness.wallpaperPreview.source(),
             QUrl::fromLocalFile(harness.wallpaperFilePath()));
}

void CustomizeWallpaperPreviewTests::tracksAuthoritativeSnapshotChanges()
{
    ModelHarness harness;
    QVERIFY(harness.establish(QStringLiteral("fixture"),
                              QStringLiteral("qindaqt:fixture-wall"),
                              QStringLiteral("scaled")));
    QCOMPARE(harness.wallpaperPreview.status(), QStringLiteral("ready"));

    // A changed Settings1 revision republishes the preview truth: identity,
    // mode, and source all follow the fresh snapshot.
    QVERIFY(harness.updateWallpaper(QStringLiteral("qindaqt:fixture-wall"),
                                    QStringLiteral("tiled"), 8));
    QTRY_COMPARE(harness.wallpaperPreview.mode(), QStringLiteral("tiled"));

    QVERIFY(harness.updateWallpaper({}, QStringLiteral("scaled"), 9));
    QTRY_COMPARE(harness.wallpaperPreview.status(), QStringLiteral("none"));
    QVERIFY(harness.wallpaperPreview.source().isEmpty());
    QCOMPARE(harness.wallpaperPreview.mode(), QStringLiteral("scaled"));
}

void CustomizeWallpaperPreviewTests::failsClosedToExplicitFallbacks()
{
    // No Settings1 truth at all: the projection stays unavailable with an
    // empty source so the canvas keeps its token gradient.
    ModelHarness offline;
    QCOMPARE(offline.wallpaperPreview.status(), QStringLiteral("unavailable"));
    QVERIFY(offline.wallpaperPreview.source().isEmpty());

    ModelHarness harness;
    // An explicit empty preference is "none", distinct from invalid truth.
    QVERIFY(harness.establish());
    QCOMPARE(harness.wallpaperPreview.status(), QStringLiteral("none"));
    QVERIFY(harness.wallpaperPreview.source().isEmpty());

    // Unknown identity, unknown mode, and a mistyped value each fail closed
    // to "invalid" with no source; the stored preference is never renamed.
    QVERIFY(harness.updateWallpaper(QStringLiteral("qindaqt:missing-wall"),
                                    QStringLiteral("scaled"), 8));
    QTRY_COMPARE(harness.wallpaperPreview.status(), QStringLiteral("invalid"));
    QVERIFY(harness.wallpaperPreview.source().isEmpty());

    QVERIFY(harness.updateWallpaper(QStringLiteral("qindaqt:fixture-wall"),
                                    QStringLiteral("diagonal"), 9));
    QTRY_COMPARE(harness.wallpaperPreview.status(), QStringLiteral("invalid"));
    QVERIFY(harness.wallpaperPreview.source().isEmpty());

    QVERIFY(harness.updateWallpaper(QStringLiteral("/does/not/exist.png"),
                                    QStringLiteral("scaled"), 10));
    QTRY_COMPARE(harness.wallpaperPreview.status(), QStringLiteral("invalid"));
    QVERIFY(harness.wallpaperPreview.source().isEmpty());

    using QindaQt::Services::SettingsProtocol::WireContract;
    const qsizetype before = harness.wallpaperTransport.snapshots.size();
    Q_EMIT harness.wallpaperTransport.settingsChanged(
        QStringLiteral(":1.90"), QStringLiteral("epoch-a"), 11,
        {QStringLiteral("appearance.wallpaper")});
    QTRY_VERIFY(harness.wallpaperTransport.snapshots.size() > before);
    const auto request = harness.wallpaperTransport.snapshots.takeLast();
    QVariantMap wire = wallpaperSnapshotWire(QStringLiteral("qindaqt:fixture-wall"),
                                             QStringLiteral("scaled"),
                                             QStringLiteral("epoch-a"), 11);
    QVariantMap values = wire.value(QLatin1StringView(WireContract::FieldValues))
                             .toMap();
    values.insert(QStringLiteral("appearance.wallpaper"), 42);
    wire.insert(QLatin1StringView(WireContract::FieldValues), values);
    Q_EMIT harness.wallpaperTransport.snapshotReceived(request.token,
                                                       request.owner, wire);
    QTRY_COMPARE(harness.wallpaperPreview.status(), QStringLiteral("invalid"));
    QVERIFY(harness.wallpaperPreview.source().isEmpty());
}

} // namespace

QTEST_GUILESS_MAIN(CustomizeWallpaperPreviewTests)
#include "tst_customize_wallpaper_preview.moc"
