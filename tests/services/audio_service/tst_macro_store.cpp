// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0183: macro buttons are a user-written document of console operations.
// These rows pin the documented action forms, that an unusable action drops
// its whole macro, and the caps.

#include <qindaqt/services/audio_service/macro_store.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;

class MacroStoreTests final : public QObject
{
    Q_OBJECT
private slots:
    void everyDocumentedActionParses();
    void anUnusableActionDropsItsMacro();
    void theDocumentIsBoundedAndFailsClosed();
};

void MacroStoreTests::everyDocumentedActionParses()
{
    const auto parse = [](const char *json) {
        return MacroStore::parseAction(QJsonDocument::fromJson(json).object());
    };
    auto mute = parse(R"({"op":"strip.mute","strip":"strip.hw.1","on":true})");
    QVERIFY(mute.has_value());
    QCOMPARE(mute->kind, OperationKind::SetStripMute);
    QVERIFY(mute->muted);
    auto send = parse(R"({"op":"strip.send","strip":"strip.hw.1","bus":2,"on":true,"gainDb":-3})");
    QVERIFY(send.has_value());
    QCOMPARE(send->busIndex, 2u);
    QCOMPARE(send->gainDb, -3.0);
    auto gain = parse(R"({"op":"bus.gain","bus":"bus.a1","gainDb":-6})");
    QVERIFY(gain.has_value());
    QCOMPARE(gain->kind, OperationKind::SetBusGain);
    auto preset = parse(R"({"op":"preset.load","name":"Podcast"})");
    QVERIFY(preset.has_value());
    QCOMPARE(preset->kind, OperationKind::LoadPreset);
    QCOMPARE(preset->displayName, QStringLiteral("Podcast"));
    // Not documented, not parsed: nothing a macro can name reaches the graph
    // slice, a virtual device, or a stream.
    QVERIFY(!parse(R"({"op":"device.default","serial":10})").has_value());
    QVERIFY(!parse(R"({"op":"strip.gain","strip":"strip.hw.1","gainDb":"loud"})").has_value());
    QVERIFY(!parse(R"({"op":"strip.send","strip":"strip.hw.1","bus":99})").has_value());
}

void MacroStoreTests::anUnusableActionDropsItsMacro()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("audio-macros.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"macros":[
        {"name":"Mute mic","actions":[{"op":"strip.mute","strip":"strip.hw.1","on":true}]},
        {"name":"Broken","actions":[{"op":"strip.mute","strip":"strip.hw.2"},{"op":"nope"}]},
        {"name":"Mute mic","actions":[{"op":"strip.mute","strip":"strip.hw.3"}]},
        {"name":"Empty","actions":[]}
    ]})");
    file.close();
    const QList<Macro> macros = MacroStore(path).load();
    QCOMPARE(MacroStore::names(macros), QStringList{QStringLiteral("Mute mic")});
    QCOMPARE(macros.at(0).actions.size(), 1);
}

void MacroStoreTests::theDocumentIsBoundedAndFailsClosed()
{
    QTemporaryDir dir;
    QVERIFY(MacroStore(dir.filePath(QStringLiteral("missing.json"))).load().isEmpty());
    const QString garbage = dir.filePath(QStringLiteral("garbage.json"));
    {
        QFile file(garbage);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[1,2]");
    }
    QVERIFY(MacroStore(garbage).load().isEmpty());
    QJsonArray many;
    for (int index = 0; index < kMaxMacros + 5; ++index) {
        many.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("m%1").arg(index)},
                                {QStringLiteral("actions"),
                                 QJsonArray{QJsonObject{{QStringLiteral("op"), QStringLiteral("bus.mute")},
                                                        {QStringLiteral("bus"), QStringLiteral("bus.a1")}}}}});
    }
    const QString capped = dir.filePath(QStringLiteral("capped.json"));
    {
        QFile file(capped);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("macros"), many}}).toJson());
    }
    QCOMPARE(MacroStore(capped).load().size(), kMaxMacros);
}

QTEST_APPLESS_MAIN(MacroStoreTests)
#include "tst_macro_store.moc"
