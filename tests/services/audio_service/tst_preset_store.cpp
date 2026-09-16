// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0182: presets are the console document under a name. These rows pin the
// name-to-file rule, the round trip, the cap, and that a hostile name can never
// become a path.

#include <qindaqt/services/audio_service/preset_store.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;

class PresetStoreTests final : public QObject
{
    Q_OBJECT
private slots:
    void namesBecomeSafeSlugs();
    void aPresetRoundTripsAndKeepsItsDisplayName();
    void theCapAndUnknownNamesFailClosed();
};

void PresetStoreTests::namesBecomeSafeSlugs()
{
    QCOMPARE(PresetStore::slugFor(QStringLiteral("Stream Night")), QStringLiteral("stream-night"));
    QCOMPARE(PresetStore::slugFor(QStringLiteral("  Podcast!!  ")), QStringLiteral("podcast"));
    // AGENT-GUARD: nothing a name contains can leave the preset directory.
    QCOMPARE(PresetStore::slugFor(QStringLiteral("../../etc/passwd")), QStringLiteral("etc-passwd"));
    QVERIFY(PresetStore::slugFor(QStringLiteral("///")).isEmpty());
    QVERIFY(PresetStore::slugFor(QString()).isEmpty());
    QVERIFY(PresetStore::slugFor(QString(kMaxPresetNameUtf8Bytes + 1, QLatin1Char('a'))).isEmpty());
}

void PresetStoreTests::aPresetRoundTripsAndKeepsItsDisplayName()
{
    QTemporaryDir dir;
    PresetStore store(dir.filePath(QStringLiteral("presets")));
    QVERIFY(store.names().isEmpty());

    ConsoleModel model;
    OperationRequest gain;
    gain.kind = OperationKind::SetStripGain;
    gain.consoleId = model.console().strips.at(0).id;
    gain.gainDb = -9.0;
    QString reason;
    QVERIFY(model.apply(gain, &reason));
    QVERIFY(store.save(QStringLiteral("Stream Night"), model));
    QCOMPARE(store.names(), QStringList{QStringLiteral("Stream Night")});

    ConsoleModel restored;
    QVERIFY(store.load(QStringLiteral("Stream Night"), restored));
    QCOMPARE(restored.console().strips.at(0).gainDb, -9.0);
    // Saving the same name replaces, never duplicates.
    QVERIFY(store.save(QStringLiteral("stream night"), model));
    QCOMPARE(store.names().size(), 1);
    QVERIFY(store.remove(QStringLiteral("Stream Night")));
    QVERIFY(store.names().isEmpty());
    QVERIFY(!store.remove(QStringLiteral("Stream Night")));
}

void PresetStoreTests::theCapAndUnknownNamesFailClosed()
{
    QTemporaryDir dir;
    PresetStore store(dir.filePath(QStringLiteral("presets")));
    ConsoleModel model;
    for (int index = 0; index < kMaxPresets; ++index) {
        QVERIFY(store.save(QStringLiteral("preset %1").arg(index), model));
    }
    QCOMPARE(store.names().size(), kMaxPresets);
    QVERIFY(!store.save(QStringLiteral("one too many"), model));
    // An existing name still saves at the cap.
    QVERIFY(store.save(QStringLiteral("preset 3"), model));
    ConsoleModel untouched;
    const Console before = untouched.console();
    QVERIFY(!store.load(QStringLiteral("never saved"), untouched));
    QCOMPARE(untouched.console(), before);
}

QTEST_APPLESS_MAIN(PresetStoreTests)
#include "tst_preset_store.moc"
