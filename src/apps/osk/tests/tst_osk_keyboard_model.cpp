// SPDX-License-Identifier: GPL-3.0-or-later
#include "osk_keyboard_model.h"
#include "osk_layout_catalog.h"
#include "osk_layout_document.h"

#include <QSignalSpy>
#include <QTest>
#include <xkbcommon/xkbcommon-keysyms.h>

using namespace QindaQt::Apps::Osk;

namespace {

class RecordingEmitter final : public KeyEmitter {
public:
    void commitText(const QString &text) override { committed.append(text); }
    void pressKeysym(quint32 keysym) override { keysyms.append(keysym); }
    QStringList committed;
    QList<quint32> keysyms;
};

int keyIndex(const OskKeyboardModel &model, int row, const QString &label)
{
    const auto rows = model.placedRows();
    const QList<PlacedKey> &keys = rows.at(row);
    for (int index = 0; index < keys.size(); ++index) {
        if (keys.at(index).label == label) {
            return index;
        }
    }
    return -1;
}

int kindIndex(const OskKeyboardModel &model, int row, KeyKind kind)
{
    const auto rows = model.placedRows();
    const QList<PlacedKey> &keys = rows.at(row);
    for (int index = 0; index < keys.size(); ++index) {
        if (keys.at(index).kind == kind) {
            return index;
        }
    }
    return -1;
}

} // namespace

class OskKeyboardModelTests final : public QObject {
    Q_OBJECT

private slots:
    void catalogShipsEveryLayoutAndFallsBackToUs();
    void documentRejectsMalformedInput();
    void placedRowsFitTheWidthAndCentre();
    void textKeysCommitAndShiftIsOneShot();
    void controlKeysForwardKeysymsAndSwitchPages();
    void layoutAndHideKeysAskTheHost();
};

void OskKeyboardModelTests::catalogShipsEveryLayoutAndFallsBackToUs()
{
    const OskLayoutCatalog catalog;
    const QStringList names = catalog.available();
    QVERIFY(names.contains(QStringLiteral("us")));
    QVERIFY(names.contains(QStringLiteral("de")));
    QVERIFY(names.contains(QStringLiteral("fr")));
    for (const QString &name : names) {
        QString error;
        const OskLayoutDocument document = catalog.documentFor(name, &error);
        QVERIFY2(document.isValid(), qPrintable(name + QStringLiteral(": ") + error));
        QCOMPARE(document.name, name);
        QVERIFY(!document.symbols.isEmpty());
    }
    QCOMPARE(OskLayoutCatalog::normalize(QStringLiteral(" DE(nodeadkeys) ")), QStringLiteral("de"));
    QCOMPARE(OskLayoutCatalog::normalize(QStringLiteral("us,de")), QStringLiteral("us"));
    QCOMPARE(catalog.documentFor(QStringLiteral("xx")).name, QStringLiteral("us"));
    QCOMPARE(catalog.documentFor(QString()).name, QStringLiteral("us"));
    QCOMPARE(catalog.documentFor(QStringLiteral("de(neo)")).name, QStringLiteral("de"));
}

void OskKeyboardModelTests::documentRejectsMalformedInput()
{
    QString error;
    QVERIFY(!OskLayoutDocument::fromJson("not json", &error).isValid());
    QVERIFY(!error.isEmpty());
    QVERIFY(!OskLayoutDocument::fromJson(R"({"letters": [["a"]]})", &error).isValid());
    QVERIFY(error.contains(QStringLiteral("name")));
    QVERIFY(!OskLayoutDocument::fromJson(R"({"name": "x", "letters": [[]]})", &error).isValid());
    QVERIFY(!OskLayoutDocument::fromJson(R"({"name": "x", "letters": [[""]]})", &error).isValid());
    QVERIFY(!OskLayoutDocument::fromJson(R"({"name": "x", "letters": [[{"text": "a", "width": 0}]]})", &error).isValid());
    const OskLayoutDocument document = OskLayoutDocument::fromJson(
        R"({"name": "X", "letters": [["a", {"text": "b", "shift": "B!", "width": 2}, "{space}"]]})", &error);
    QVERIFY2(document.isValid(), qPrintable(error));
    QCOMPARE(document.name, QStringLiteral("x"));
    QCOMPARE(document.label, QStringLiteral("X"));
    QCOMPARE(document.letters.at(0).at(1).shiftedText, QStringLiteral("B!"));
    QCOMPARE(document.letters.at(0).at(1).width, 2.0);
    QCOMPARE(document.letters.at(0).at(2).kind, KeyKind::Space);
    QCOMPARE(document.letters.at(0).at(2).width, 5.0);
}

void OskKeyboardModelTests::placedRowsFitTheWidthAndCentre()
{
    OskKeyboardModel model;
    model.setDocument(OskLayoutCatalog().documentFor(QStringLiteral("us")));
    model.relayout(1000.0, 50.0, 6.0, 10.0);
    const auto rows = model.placedRows();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(model.panelHeight(), 10.0 * 2 + 4 * 50.0 + 3 * 6.0);
    for (const QList<PlacedKey> &row : rows) {
        QVERIFY(row.constFirst().rect.left() >= 10.0);
        QVERIFY(row.constLast().rect.right() <= 990.0 + 0.001);
        for (int index = 1; index < row.size(); ++index) {
            QCOMPARE(row.at(index).rect.left(), row.at(index - 1).rect.right() + 6.0);
        }
    }
    // Ten letter keys share the first row symmetrically.
    const QList<PlacedKey> &first = rows.at(0);
    QCOMPARE(first.size(), 10);
    QVERIFY(qAbs((first.constFirst().rect.left() - 10.0) - (990.0 - first.constLast().rect.right())) < 0.001);
    const auto hit = model.keyAt(rows.at(0).at(2).rect.center());
    QVERIFY(hit.has_value());
    QCOMPARE(hit->label, QStringLiteral("e"));
    QVERIFY(!model.keyAt(QPointF(-1.0, -1.0)).has_value());
    // A wide panel caps the unit so keys never balloon.
    model.relayout(4000.0, 50.0, 6.0, 10.0);
    QVERIFY(model.placedRows().at(0).constFirst().rect.width() <= model.metrics().maximumUnit + 0.001);
}

void OskKeyboardModelTests::textKeysCommitAndShiftIsOneShot()
{
    OskKeyboardModel model;
    RecordingEmitter emitter;
    model.setEmitter(&emitter);
    model.setDocument(OskLayoutCatalog().documentFor(QStringLiteral("us")));
    model.relayout(800.0, 48.0, 4.0, 8.0);
    QSignalSpy pressed(&model, &OskKeyboardModel::keyPressed);
    model.press(0, keyIndex(model, 0, QStringLiteral("q")));
    QCOMPARE(emitter.committed, QStringList{QStringLiteral("q")});
    const int shift = kindIndex(model, 2, KeyKind::Shift);
    QVERIFY(shift >= 0);
    model.press(2, shift);
    QVERIFY(model.shifted());
    QCOMPARE(model.placedRows().at(0).at(0).label, QStringLiteral("Q"));
    model.press(0, 0);
    QCOMPARE(emitter.committed.last(), QStringLiteral("Q"));
    QVERIFY(!model.shifted());
    QCOMPARE(model.placedRows().at(0).at(0).label, QStringLiteral("q"));
    model.press(3, kindIndex(model, 3, KeyKind::Space));
    QCOMPARE(emitter.committed.last(), QStringLiteral(" "));
    QCOMPARE(pressed.count(), 4);
    QCOMPARE(pressed.at(0).at(0).toString(), QStringLiteral("text"));
    QCOMPARE(pressed.at(0).at(1).toString(), QStringLiteral("q"));
    model.press(9, 9);
    QCOMPARE(pressed.count(), 4);
}

void OskKeyboardModelTests::controlKeysForwardKeysymsAndSwitchPages()
{
    OskKeyboardModel model;
    RecordingEmitter emitter;
    model.setEmitter(&emitter);
    model.setDocument(OskLayoutCatalog().documentFor(QStringLiteral("us")));
    model.relayout(800.0, 48.0, 4.0, 8.0);
    model.press(2, kindIndex(model, 2, KeyKind::Backspace));
    model.press(3, kindIndex(model, 3, KeyKind::Enter));
    QCOMPARE(emitter.keysyms, (QList<quint32>{XKB_KEY_BackSpace, XKB_KEY_Return}));
    QVERIFY(emitter.committed.isEmpty());
    model.press(3, kindIndex(model, 3, KeyKind::Symbols));
    QVERIFY(model.symbolsPage());
    QCOMPARE(model.placedRows().at(0).at(0).label, QStringLiteral("1"));
    QCOMPARE(model.placedRows().at(2).at(1).label, QStringLiteral("*"));
    model.press(2, kindIndex(model, 2, KeyKind::Shift));
    QCOMPARE(model.placedRows().at(2).at(1).label, QStringLiteral("~"));
    model.press(2, 1);
    QCOMPARE(emitter.committed, QStringList{QStringLiteral("~")});
    QVERIFY(!model.shifted());
    model.press(3, kindIndex(model, 3, KeyKind::Letters));
    QVERIFY(!model.symbolsPage());
    model.showSymbols(true);
    QVERIFY(model.symbolsPage());
    model.resetTransientState();
    QVERIFY(!model.symbolsPage());
    QVERIFY(!model.shifted());
}

void OskKeyboardModelTests::layoutAndHideKeysAskTheHost()
{
    OskKeyboardModel model;
    model.setDocument(OskLayoutCatalog().documentFor(QStringLiteral("de")));
    model.relayout(800.0, 48.0, 4.0, 8.0);
    QCOMPARE(model.layoutLabel(), QStringLiteral("DE"));
    QCOMPARE(model.placedRows().at(0).at(5).label, QStringLiteral("z"));
    QSignalSpy hide(&model, &OskKeyboardModel::hideRequested);
    QSignalSpy layout(&model, &OskKeyboardModel::nextLayoutRequested);
    model.press(3, kindIndex(model, 3, KeyKind::Hide));
    model.press(3, kindIndex(model, 3, KeyKind::Layout));
    QCOMPARE(hide.count(), 1);
    QCOMPARE(layout.count(), 1);
    QCOMPARE(model.placedRows().at(3).at(kindIndex(model, 3, KeyKind::Layout)).label, QStringLiteral("DE"));
}

QTEST_GUILESS_MAIN(OskKeyboardModelTests)
#include "tst_osk_keyboard_model.moc"
