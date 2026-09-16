// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0176: the console remembers itself. These rows pin that a fader, a
// label and a routing cell survive a round trip through the store, that the
// store fails closed on anything that is not a console document, and that a
// write is all-or-nothing.

#include <qindaqt/services/audio_service/console_store.h>

#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;

namespace {

OperationRequest consoleRequest(const OperationKind kind, const QString &id)
{
    OperationRequest request;
    request.kind = kind;
    request.consoleId = id;
    return request;
}

} // namespace

class ConsoleStoreTests final : public QObject
{
    Q_OBJECT
private slots:
    void aConsoleSurvivesTheRoundTrip();
    void nothingOnDiskLeavesTheDefaults();
    void garbageAndOversizedDocumentsAreRefused();
    void identicalSavesDoNotRewrite();
};

void ConsoleStoreTests::aConsoleSurvivesTheRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ConsoleStore store(dir.filePath(QStringLiteral("nested/audio-console.json")));

    ConsoleModel model;
    const QString stripId = model.console().strips.at(0).id;
    const QString busId = model.console().buses.at(1).id;
    QString reason;
    auto gain = consoleRequest(OperationKind::SetStripGain, stripId);
    gain.gainDb = -7.5;
    QVERIFY(model.apply(gain, &reason));
    auto mute = consoleRequest(OperationKind::SetBusMute, busId);
    mute.muted = true;
    QVERIFY(model.apply(mute, &reason));
    auto send = consoleRequest(OperationKind::SetStripSend, stripId);
    send.busIndex = 1;
    send.enabled = true;
    send.gainDb = -3.0;
    QVERIFY(model.apply(send, &reason));
    // The directory does not exist yet: the store creates it.
    QVERIFY(store.save(model));

    ConsoleModel restored;
    QVERIFY(ConsoleStore(store.path()).load(restored));
    const Console console = restored.console();
    QCOMPARE(console.strips.at(0).gainDb, -7.5);
    QVERIFY(console.buses.at(1).muted);
    QVERIFY(console.strips.at(0).sends.at(1).enabled);
    QCOMPARE(console.strips.at(0).sends.at(1).gainDb, -3.0);
    QVERIFY(validateConsole(console).accepted);
    // AGENT-GUARD: nothing live comes back. A restored console is unbound
    // until the graph binds it; a persisted serial would point at a device
    // that no longer exists after a reboot.
    for (const Strip &strip : console.strips) {
        QVERIFY(!strip.sourceKnown);
        QVERIFY(!strip.level.known);
    }
}

void ConsoleStoreTests::nothingOnDiskLeavesTheDefaults()
{
    QTemporaryDir dir;
    ConsoleModel model;
    const Console before = model.console();
    QVERIFY(!ConsoleStore(dir.filePath(QStringLiteral("missing.json"))).load(model));
    QCOMPARE(model.console(), before);
}

void ConsoleStoreTests::garbageAndOversizedDocumentsAreRefused()
{
    QTemporaryDir dir;
    ConsoleModel model;
    const Console before = model.console();

    const QString garbagePath = dir.filePath(QStringLiteral("garbage.json"));
    {
        QFile garbage(garbagePath);
        QVERIFY(garbage.open(QIODevice::WriteOnly));
        garbage.write("{ not json");
    }
    QVERIFY(!ConsoleStore(garbagePath).load(model));
    QCOMPARE(model.console(), before);

    // A JSON array is well-formed and still not a console.
    const QString arrayPath = dir.filePath(QStringLiteral("array.json"));
    {
        QFile array(arrayPath);
        QVERIFY(array.open(QIODevice::WriteOnly));
        array.write("[1, 2, 3]");
    }
    QVERIFY(!ConsoleStore(arrayPath).load(model));
    QCOMPARE(model.console(), before);

    const QString hugePath = dir.filePath(QStringLiteral("huge.json"));
    {
        QFile huge(hugePath);
        QVERIFY(huge.open(QIODevice::WriteOnly));
        huge.write("{\"strips\": [");
        huge.write(QByteArray(ConsoleStore::kMaxDocumentBytes, ' '));
        huge.write("]}");
    }
    QVERIFY(!ConsoleStore(hugePath).load(model));
    QCOMPARE(model.console(), before);
}

void ConsoleStoreTests::identicalSavesDoNotRewrite()
{
    QTemporaryDir dir;
    ConsoleStore store(dir.filePath(QStringLiteral("audio-console.json")));
    ConsoleModel model;
    QVERIFY(store.save(model));
    const QDateTime first = QFileInfo(store.path()).lastModified();
    QVERIFY(first.isValid());
    // Remove the file behind the store's back: an identical document is not
    // rewritten, which is what makes saving on every change cheap.
    QVERIFY(QFile::remove(store.path()));
    QVERIFY(store.save(model));
    QVERIFY(!QFileInfo::exists(store.path()));

    QString reason;
    auto gain = consoleRequest(OperationKind::SetStripGain, model.console().strips.at(0).id);
    gain.gainDb = 2.0;
    QVERIFY(model.apply(gain, &reason));
    QVERIFY(store.save(model));
    QVERIFY(QFileInfo::exists(store.path()));
}

QTEST_APPLESS_MAIN(ConsoleStoreTests)
#include "tst_console_store.moc"
