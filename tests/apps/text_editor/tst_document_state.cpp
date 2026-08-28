// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/document_state.h"

#include <QTest>

using namespace QindaQt::Apps::TextEditor;

class DocumentStateTest final : public QObject {
  Q_OBJECT

private slots:
  void untitledLifecycle();
  void loadedDocumentTracksExactDirtyState();
  void incrementalEditsKeepExactDirtyTruth();
  void saveAndExternalStateAreIndependent();
};

void DocumentStateTest::untitledLifecycle() {
  DocumentState state;
  QVERIFY(state.isUntitled());
  QVERIFY(!state.isDirty());
  QCOMPARE(state.externalState(), ExternalState::InSync);

  QVERIFY(state.setText(QStringLiteral("hello")));
  QVERIFY(state.isDirty());
  QVERIFY(!state.setText(QStringLiteral("hello")));
  state.reset();
  QVERIFY(state.isUntitled());
  QVERIFY(!state.isDirty());
  QVERIFY(state.text().isEmpty());
}

void DocumentStateTest::incrementalEditsKeepExactDirtyTruth() {
  DocumentState state;
  state.load(QStringLiteral("/tmp/example.txt"),
             {.text = QStringLiteral("hello\nworld"),
              .revision = {.exists = true,
                           .byteCount = 11,
                           .sha256 = QByteArray(32, 'c')}});

  QVERIFY(state.applyTextEdit(5, 0, QStringLiteral("!")));
  QCOMPARE(state.text(), QStringLiteral("hello!\nworld"));
  QVERIFY(state.isDirty());
  QVERIFY(state.applyTextEdit(5, 1, {}));
  QCOMPARE(state.text(), QStringLiteral("hello\nworld"));
  QVERIFY(!state.isDirty());

  QVERIFY(state.applyTextEdit(0, 1, QStringLiteral("H")));
  QVERIFY(state.isDirty());
  QVERIFY(state.applyTextEdit(0, 1, QStringLiteral("h")));
  QVERIFY(!state.isDirty());
  QVERIFY(!state.applyTextEdit(-1, 0, QStringLiteral("invalid")));
  QCOMPARE(state.text(), QStringLiteral("hello\nworld"));
}

void DocumentStateTest::loadedDocumentTracksExactDirtyState() {
  const FileRevision revision{
      .exists = true, .byteCount = 5, .sha256 = QByteArray(32, 'a')};
  DocumentState state;
  state.load(QStringLiteral("/tmp/example.txt"),
             {.text = QStringLiteral("hello"),
              .hasUtf8Bom = true,
              .lineEnding = LineEnding::CrLf,
              .revision = revision});
  QVERIFY(!state.isUntitled());
  QVERIFY(!state.isDirty());
  QVERIFY(state.hasUtf8Bom());
  QCOMPARE(state.lineEnding(), LineEnding::CrLf);
  QVERIFY(state.baselineRevision().has_value());
  QVERIFY(*state.baselineRevision() == revision);

  QVERIFY(state.setText(QStringLiteral("changed")));
  QVERIFY(state.isDirty());
  QVERIFY(state.setText(QStringLiteral("hello")));
  QVERIFY(!state.isDirty());
}

void DocumentStateTest::saveAndExternalStateAreIndependent() {
  DocumentState state;
  QVERIFY(state.setText(QStringLiteral("draft")));
  const FileRevision revision{
      .exists = true, .byteCount = 5, .sha256 = QByteArray(32, 'b')};
  state.markSaved(QStringLiteral("/tmp/draft.txt"), revision);
  QVERIFY(!state.isDirty());
  QCOMPARE(state.externalState(), ExternalState::InSync);

  QVERIFY(state.setExternalState(ExternalState::Changed));
  QVERIFY(!state.setExternalState(ExternalState::Changed));
  QVERIFY(!state.isDirty());
  QVERIFY(state.setText(QStringLiteral("local edit")));
  QVERIFY(state.isDirty());
  QCOMPARE(state.externalState(), ExternalState::Changed);
}

QTEST_APPLESS_MAIN(DocumentStateTest)
#include "tst_document_state.moc"
