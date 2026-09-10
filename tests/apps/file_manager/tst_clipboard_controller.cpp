// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/clipboard_controller.h"

#include "mutation/mutation_controller.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QMutex>
#include <QMutexLocker>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace QindaQt::Apps::FileManager;

namespace {

// Slim batch recorder: the backend runs on the mutation worker thread, so all
// recorded state is mutex-guarded. Succeeds unless a failure is injected.
class RecordingBackend final : public MutationBackend {
public:
  [[nodiscard]] MutationResult
  execute(const MutationRequest &request,
          const MutationCancellation & /*cancellation*/,
          const MutationProgressCallback & /*progress*/) override {
    {
      QMutexLocker lock(&m_mutex);
      m_requests.append(request);
    }
    if (failNext.exchange(false)) {
      MutationResult failure;
      failure.error = MutationError::IoError;
      failure.diagnostic = QStringLiteral("injected failure");
      return failure;
    }
    MutationResult result;
    result.outputPath = request.destinationPath;
    return result;
  }

  [[nodiscard]] int requestCount() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_requests.size());
  }

  [[nodiscard]] MutationRequest recordedRequest(int index) const {
    QMutexLocker lock(&m_mutex);
    return m_requests.at(index);
  }

  std::atomic_bool failNext = false;

private:
  mutable QMutex m_mutex;
  QVector<MutationRequest> m_requests;
};

// AGENT-CONTRACT: Own-clipboard snapshots are the QML entry maps — a "path"
// string plus decimal-string identity fields identityFromMap() consumes.
[[nodiscard]] QVariantMap entrySnapshot(const QString &path, quint64 inode) {
  return {{QStringLiteral("path"), path},
          {QStringLiteral("device"), QStringLiteral("1")},
          {QStringLiteral("inode"), QString::number(inode)},
          {QStringLiteral("identitySize"), QStringLiteral("3")},
          {QStringLiteral("modifiedNanoseconds"), QStringLiteral("4")},
          {QStringLiteral("mode"), QStringLiteral("5")}};
}

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(contents) == contents.size();
}

} // namespace

class TestClipboardController final : public QObject {
  Q_OBJECT

private slots:
  void init();
  void copyArmsOwnSnapshotAndPublishesInteropPayload();
  void cutArmsCutModeAndInteropMarkers();
  void copyPasteDispatchesCopyAndKeepsSnapshot();
  void cutPasteDispatchesMoveAndClearsAfterSuccess();
  void failedCutPasteKeepsTheSnapshot();
  void foreignClipboardIsAdoptedAsCopyOnly();
  void dropUrlsIntoAcceptsLocalFilesOnly();
  void pasteRejectsInvalidDestinations();
  void pasteIntoSelfOrDescendantIsRejected();
};

void TestClipboardController::init() {
  // The process-wide clipboard outlives each test's controller; isolate cases
  // from whatever the previous case (or the environment) left behind.
  QGuiApplication::clipboard()->clear(QClipboard::Clipboard);
  QCoreApplication::processEvents();
}

void TestClipboardController::copyArmsOwnSnapshotAndPublishesInteropPayload() {
  auto backendHolder = std::make_unique<RecordingBackend>();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  QVERIFY(!clipboard.canPaste());
  QCOMPARE(clipboard.mode(), QStringLiteral("none"));
  QVERIFY(!clipboard.copySelection({}));
  QVERIFY(!clipboard.lastRejection().isEmpty());

  QVERIFY(clipboard.copySelection(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 11),
       entrySnapshot(QStringLiteral("/fixture/b.txt"), 12)}));
  QVERIFY(clipboard.canPaste());
  QCOMPARE(clipboard.mode(), QStringLiteral("copy"));
  QCOMPARE(clipboard.count(), 2);

  // Mirrored for the action-enable binding in file_manager_transfer_actions.
  clipboard.setSelectionCount(2);
  QCOMPARE(clipboard.selectionCount(), 2);

  // Offscreen-QPA regression: our own publish must not be re-adopted as
  // foreign content when the platform reports no ownership change.
  QCoreApplication::processEvents();
  QCOMPARE(clipboard.mode(), QStringLiteral("copy"));
  QVERIFY(clipboard.canPaste());

  const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
  QVERIFY(mime != nullptr);
  QVERIFY(mime->hasUrls());
  QCOMPARE(mime->urls().size(), 2);
  const QByteArray gnome =
      mime->data(QStringLiteral("x-special/gnome-copied-files"));
  QVERIFY(gnome.startsWith("copy\n"));
  QVERIFY(gnome.contains("file:///fixture/a.txt"));
  QVERIFY(!mime->hasFormat(QStringLiteral("application/x-kde-cutselection")));
}

void TestClipboardController::cutArmsCutModeAndInteropMarkers() {
  auto backendHolder = std::make_unique<RecordingBackend>();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  QVERIFY(clipboard.cutSelection(
      {entrySnapshot(QStringLiteral("/fixture/a.txt"), 13)}));
  QCOMPARE(clipboard.mode(), QStringLiteral("cut"));
  QCOMPARE(clipboard.count(), 1);

  QCoreApplication::processEvents();
  QCOMPARE(clipboard.mode(), QStringLiteral("cut"));

  const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
  QVERIFY(mime != nullptr);
  QVERIFY(mime->data(QStringLiteral("x-special/gnome-copied-files"))
              .startsWith("cut\n"));
  QCOMPARE(mime->data(QStringLiteral("application/x-kde-cutselection")),
           QByteArray("1"));
}

void TestClipboardController::copyPasteDispatchesCopyAndKeepsSnapshot() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("dest"))));
  const QString destination = fixture.filePath(QStringLiteral("dest"));

  auto backendHolder = std::make_unique<RecordingBackend>();
  RecordingBackend *backend = backendHolder.get();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  QVERIFY(clipboard.copySelection(
      {entrySnapshot(fixture.filePath(QStringLiteral("a.txt")), 21)}));
  QVERIFY(clipboard.pasteInto(destination));
  QTRY_VERIFY_WITH_TIMEOUT(!mutation.busy(), 2000);

  QCOMPARE(backend->requestCount(), 1);
  const MutationRequest request = backend->recordedRequest(0);
  QCOMPARE(request.kind, MutationKind::Copy);
  QCOMPARE(request.sourcePath, fixture.filePath(QStringLiteral("a.txt")));
  QCOMPARE(request.destinationPath,
           destination + QStringLiteral("/a.txt"));
  QCOMPARE(mutation.failureCode(), QStringLiteral("none"));

  // A copy paste keeps its snapshot for further pastes.
  QVERIFY(clipboard.canPaste());
  QCOMPARE(clipboard.mode(), QStringLiteral("copy"));
}

void TestClipboardController::cutPasteDispatchesMoveAndClearsAfterSuccess() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("dest"))));
  const QString destination = fixture.filePath(QStringLiteral("dest"));

  auto backendHolder = std::make_unique<RecordingBackend>();
  RecordingBackend *backend = backendHolder.get();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  QVERIFY(clipboard.cutSelection(
      {entrySnapshot(fixture.filePath(QStringLiteral("a.txt")), 22)}));
  QVERIFY(clipboard.pasteInto(destination));
  // The snapshot stays armed while the move is in flight...
  QVERIFY(clipboard.canPaste());
  QTRY_VERIFY_WITH_TIMEOUT(!mutation.busy(), 2000);

  QCOMPARE(backend->requestCount(), 1);
  QCOMPARE(backend->recordedRequest(0).kind, MutationKind::Move);
  QCOMPARE(mutation.failureCode(), QStringLiteral("none"));

  // ...and a committed cut paste clears it so dangling sources are never
  // pasted twice.
  QTRY_VERIFY_WITH_TIMEOUT(!clipboard.canPaste(), 2000);
  QCOMPARE(clipboard.mode(), QStringLiteral("none"));
  QCOMPARE(clipboard.count(), 0);
}

void TestClipboardController::failedCutPasteKeepsTheSnapshot() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("dest"))));

  auto backendHolder = std::make_unique<RecordingBackend>();
  RecordingBackend *backend = backendHolder.get();
  backend->failNext = true;
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  QVERIFY(clipboard.cutSelection(
      {entrySnapshot(fixture.filePath(QStringLiteral("a.txt")), 23)}));
  QVERIFY(clipboard.pasteInto(fixture.filePath(QStringLiteral("dest"))));
  QTRY_VERIFY_WITH_TIMEOUT(!mutation.busy(), 2000);
  QVERIFY(mutation.failureCode() != QLatin1String("none"));

  // A failed cut paste keeps the snapshot so the user can retry elsewhere.
  QVERIFY(clipboard.canPaste());
  QCOMPARE(clipboard.mode(), QStringLiteral("cut"));
}

void TestClipboardController::foreignClipboardIsAdoptedAsCopyOnly() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("dest"))));
  const QString one = fixture.filePath(QStringLiteral("one.txt"));
  const QString two = fixture.filePath(QStringLiteral("two.txt"));
  QVERIFY(writeFile(one, "1"));
  QVERIFY(writeFile(two, "2"));

  auto backendHolder = std::make_unique<RecordingBackend>();
  RecordingBackend *backend = backendHolder.get();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());
  QVERIFY(!clipboard.canPaste());

  // Another application owns this payload; even a foreign *cut* marker must
  // dispatch as a copy — foreign sources are never moved out from under
  // their owner.
  auto *mime = new QMimeData;
  mime->setUrls({QUrl::fromLocalFile(one), QUrl::fromLocalFile(two)});
  mime->setData(QStringLiteral("x-special/gnome-copied-files"),
                QByteArray("cut\n") + QUrl::fromLocalFile(one).toEncoded() +
                    '\n' + QUrl::fromLocalFile(two).toEncoded() + '\n');
  QGuiApplication::clipboard()->setMimeData(mime);
  QTRY_VERIFY_WITH_TIMEOUT(clipboard.canPaste(), 1000);
  QCOMPARE(clipboard.mode(), QStringLiteral("copy"));
  QCOMPARE(clipboard.count(), 2);

  QVERIFY(clipboard.pasteInto(fixture.filePath(QStringLiteral("dest"))));
  QTRY_VERIFY_WITH_TIMEOUT(!mutation.busy(), 2000);
  QCOMPARE(backend->requestCount(), 2);
  QCOMPARE(backend->recordedRequest(0).kind, MutationKind::Copy);
  QCOMPARE(backend->recordedRequest(1).kind, MutationKind::Copy);
  // Foreign paths are stat'ed at dispatch: the identity-checked contract is
  // preserved without any listing-time snapshot.
  QVERIFY(backend->recordedRequest(0).expectedSource.has_value());
  QVERIFY(QFile::exists(one));
  QVERIFY(QFile::exists(two));
}

void TestClipboardController::dropUrlsIntoAcceptsLocalFilesOnly() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("dest"))));
  const QString first = fixture.filePath(QStringLiteral("first.txt"));
  const QString second = fixture.filePath(QStringLiteral("second.txt"));
  QVERIFY(writeFile(first, "1"));
  QVERIFY(writeFile(second, "2"));

  auto backendHolder = std::make_unique<RecordingBackend>();
  RecordingBackend *backend = backendHolder.get();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  // Non-local URLs are skipped and duplicates collapse; drops always copy.
  QVERIFY(clipboard.dropUrlsInto(
      {QUrl::fromLocalFile(first), QUrl(QStringLiteral("https://example.test/x")),
       QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)},
      fixture.filePath(QStringLiteral("dest"))));
  QTRY_VERIFY_WITH_TIMEOUT(!mutation.busy(), 2000);
  QCOMPARE(backend->requestCount(), 2);
  QCOMPARE(backend->recordedRequest(0).kind, MutationKind::Copy);
  QCOMPARE(backend->recordedRequest(1).kind, MutationKind::Copy);

  QVERIFY(!clipboard.dropUrlsInto(
      {QUrl(QStringLiteral("https://example.test/only"))},
      fixture.filePath(QStringLiteral("dest"))));
  QVERIFY(!clipboard.lastRejection().isEmpty());
  QVERIFY(!clipboard.dropUrlsInto({QUrl::fromLocalFile(first)},
                                  QStringLiteral("relative/dest")));
  QCOMPARE(backend->requestCount(), 2);
}

void TestClipboardController::pasteRejectsInvalidDestinations() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(writeFile(fixture.filePath(QStringLiteral("a.txt")), "1"));

  auto backendHolder = std::make_unique<RecordingBackend>();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  // Nothing pasteable at all.
  QVERIFY(!clipboard.pasteInto(fixture.path()));
  QVERIFY(!clipboard.lastRejection().isEmpty());

  QVERIFY(clipboard.copySelection(
      {entrySnapshot(fixture.filePath(QStringLiteral("a.txt")), 31)}));
  QVERIFY(!clipboard.pasteInto(QStringLiteral("relative/path")));
  QVERIFY(!clipboard.lastRejection().isEmpty());
  // A regular file is not a folder destination.
  QVERIFY(!clipboard.pasteInto(fixture.filePath(QStringLiteral("a.txt"))));
  QVERIFY(!clipboard.lastRejection().isEmpty());
}

void TestClipboardController::pasteIntoSelfOrDescendantIsRejected() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("sub/inner"))));
  QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("sibling"))));
  const QString sub = fixture.filePath(QStringLiteral("sub"));

  auto backendHolder = std::make_unique<RecordingBackend>();
  RecordingBackend *backend = backendHolder.get();
  MutationController mutation(std::move(backendHolder));
  ClipboardController clipboard(mutation, *QGuiApplication::clipboard());

  QVERIFY(clipboard.copySelection({entrySnapshot(sub, 41)}));
  // Onto itself, into its own descendant, and back onto its parent are all
  // refused before dispatch.
  QVERIFY(!clipboard.pasteInto(sub));
  QVERIFY(!clipboard.lastRejection().isEmpty());
  QVERIFY(!clipboard.pasteInto(fixture.filePath(QStringLiteral("sub/inner"))));
  QVERIFY(!clipboard.pasteInto(fixture.path()));
  QCOMPARE(backend->requestCount(), 0);

  // Positive control: an unrelated folder accepts the same snapshot.
  QVERIFY(clipboard.pasteInto(fixture.filePath(QStringLiteral("sibling"))));
  QTRY_VERIFY_WITH_TIMEOUT(!mutation.busy(), 2000);
  QCOMPARE(backend->requestCount(), 1);
  QCOMPARE(backend->recordedRequest(0).kind, MutationKind::Copy);
}

QTEST_MAIN(TestClipboardController)
#include "tst_clipboard_controller.moc"
