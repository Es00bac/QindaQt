// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/audio_applet_qml_fixture.h"
#include <QScreen>
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AudioAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

class AudioStreamLayoutTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void freshStreamControlsDoNotOverlap_data() {
        QTest::addColumn<bool>("known");
        QTest::addColumn<double>("level");
        QTest::newRow("zero") << true << 0.0;
        QTest::newRow("half") << true << 0.5;
        QTest::newRow("full") << true << 1.0;
        QTest::newRow("unknown") << false << 0.0;
    }
    void freshStreamControlsDoNotOverlap() {
        QFETCH(bool, known);
        QFETCH(double, level);
        auto snapshot = clientSnapshot();
        snapshot.streams[0].applicationName =
            QStringLiteral("A long application name <b>literal</b> that must elide");
        snapshot.streams[0].volumeKnown = known;
        snapshot.streams[0].volume = level;
        FakeAudioTransport transport;
        Audio::AudioClient client(&transport);
        AudioAppletController controller(&client, true, true);
        client.start();
        transport.announceOwner(kOwner);
        transport.reply(transport.fetches.constLast(), snapshot);
        QCOMPARE(client.state(), Audio::ClientState::Ready);
        QCOMPARE(countPendingRows(controller), 0);
        AppletHarness harness;
        QString error;
        QVERIFY2(loadApplet(harness, &controller,
            {QStringLiteral("audio-volume-medium")}, &error), qPrintable(error));
        auto *root = harness.root();
        QVERIFY(root);
        QQuickWindow window;
        window.setGeometry(window.screen()->availableGeometry());
        root->setParentItem(window.contentItem());
        root->setPosition(QPointF(20, 20));
        root->setSize(QSizeF(32, 28));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto summaries = visualItemsNamed(root, QStringLiteral("audioAppletSummary"));
        QCOMPARE(summaries.size(), 1);
        summaries[0]->forceActiveFocus();
        auto *content = openPopupContent(root, &window);
        QVERIFY(content);
        const auto names = visualItemsNamed(content, QStringLiteral("audioStreamName"));
        QVERIFY(!names.isEmpty());
        auto *name = names[0];
        auto *row = name->parentItem()->parentItem();
        QVERIFY(row);
        const auto one = [row](const char *key) {
            const auto items = visualItemsNamed(row, QString::fromLatin1(key));
            return items.size() == 1 ? items[0] : nullptr;
        };
        auto *slider = one("audioStreamVolume");
        auto *percent = one("audioStreamVolumePercent");
        auto *mute = one("audioStreamMute");
        auto *unknown = one("audioStreamVolumeUnknown");
        QVERIFY(slider && percent && mute && unknown);
        QTRY_VERIFY(row->width() > 250);
        QTRY_VERIFY(mute->height() >= 36);
        const auto bounds = [row](QQuickItem *item) {
            return QRectF(item->mapToItem(row, QPointF{}), item->size());
        };
        const auto a = bounds(slider), b = bounds(percent), c = bounds(mute);
        qInfo() << "stream-bounds" << row->size() << a << b << c;
        if (known) {
            QVERIFY(slider->isVisible());
            QVERIFY(percent->isVisible());
            QVERIFY(a.width() >= 36);
            QVERIFY2(a.right() <= b.left() + 0.5, "slider and percent overlap");
            QVERIFY2(b.right() <= c.left() + 0.5, "percent and mute overlap");
            QVERIFY(a.left() >= -0.5 && c.right() <= row->width() + 0.5);
        } else {
            QVERIFY(!slider->isVisible());
            QVERIFY(!percent->isVisible());
            QVERIFY(unknown->isVisible());
            QVERIFY(bounds(unknown).right() <= c.left() + 0.5);
        }
        QCOMPARE(transport.operations.size(), 0);
    }
    void freshStreamHeaderIsSingleLineAndAccessible() {
        // Independent header regression: reuse the actual fresh popup fixture,
        // then observe text layout rather than implementation width constants.
        FakeAudioTransport transport;
        Audio::AudioClient client(&transport);
        AudioAppletController controller(&client, true, true);
        auto snapshot = clientSnapshot();
        const auto label = QStringLiteral("A long application name <b>literal</b> that must elide");
        snapshot.streams[0].applicationName = label;
        client.start(); transport.announceOwner(kOwner);
        transport.reply(transport.fetches.constLast(), snapshot);
        QCOMPARE(client.state(), Audio::ClientState::Ready);
        AppletHarness harness; QString error;
        QVERIFY2(loadApplet(harness, &controller,
            {QStringLiteral("audio-volume-medium")}, &error), qPrintable(error));
        QQuickWindow window;
        window.setGeometry(window.screen()->availableGeometry());
        auto *root = harness.root(); QVERIFY(root);
        root->setParentItem(window.contentItem());
        root->setPosition(QPointF(20, 20)); root->setSize(QSizeF(32, 28));
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto summaries = visualItemsNamed(root, QStringLiteral("audioAppletSummary"));
        QCOMPARE(summaries.size(), 1); summaries[0]->forceActiveFocus();
        auto *content = openPopupContent(root, &window); QVERIFY(content);
        const auto names = visualItemsNamed(content, QStringLiteral("audioStreamName"));
        QVERIFY(!names.isEmpty());
        auto *name = names[0];
        QTRY_VERIFY(name->width() > 0);
        QTRY_COMPARE(name->property("lineCount").toInt(), 1);
        auto *accessible = QAccessible::queryAccessibleInterface(name);
        QVERIFY(accessible);
        QCOMPARE(accessible->text(QAccessible::Name), label);
        QCOMPARE(name->property("textFormat").toInt(), int(Qt::PlainText));
        auto *row = name->parentItem()->parentItem();
        const auto directions = visualItemsNamed(row, QStringLiteral("audioStreamDirection"));
        QCOMPARE(directions.size(), 1);
        auto *direction = directions[0];
        QTRY_COMPARE(direction->property("lineCount").toInt(), 1);
        auto *directionAccessible = QAccessible::queryAccessibleInterface(direction);
        QVERIFY(directionAccessible);
        QCOMPARE(directionAccessible->text(QAccessible::Name),
                 direction->property("text").toString());
        const auto namesEnd = name->mapToItem(row, QPointF(name->width(), 0)).x();
        const auto directionStart = direction->mapToItem(row, QPointF{}).x();
        QVERIFY(namesEnd <= directionStart + 0.5);
        QCOMPARE(transport.operations.size(), 0);
    }
};
QTEST_MAIN(AudioStreamLayoutTests)
#include "tst_audio_stream_layout.moc"
