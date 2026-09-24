// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "public/desktop_file_boundary.h"
#include "runtime/file_manager1_service.h"
#include "runtime/process_reveal_windows.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWindow>

#include <memory>
#include <utility>

using namespace QindaQt::Apps::FileManager;

namespace {

// The window factory stand-in: records every request and answers `accept`.
class RecordingWindows final : public RevealWindows {
public:
  struct Shown final {
    RevealRequest request;
    QString token;
  };

  [[nodiscard]] bool show(const RevealRequest &request,
                          const QString &activationToken) override {
    shown.append({request, activationToken});
    return accept;
  }

  QList<Shown> shown;
  bool accept = true;
};

// Stands in for ui/EntryReveal.qml, which ProcessRevealWindows finds by name.
class FakeEntryReveal final : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;

  Q_INVOKABLE QVariant reveal(const QVariant &folder, const QVariant &names,
                              const QVariant &action) {
    calls.append({folder.toString(), names.toStringList(), action.toString()});
    return true;
  }

  QList<RevealRequest> calls;
};

[[nodiscard]] bool writeFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write("payload") == 7;
}

[[nodiscard]] QString uriOf(const QString &path) {
  return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

[[nodiscard]] QString canonical(const QString &path) {
  return QFileInfo(path).canonicalFilePath();
}

} // namespace

// ADR-0273: org.freedesktop.FileManager1 on a private session bus (the row
// runs under dbus-run-session with a bus that has no activation directories,
// so no host file manager can be started) with a recording window factory,
// and the production factory's window reuse rule.
class TestFileManager1Service final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void servesTheFreedesktopNameAndQueuesTheNextProcess();
  void showItemsGroupsByFolderAndHandsTheTokenToTheFirstWindow();
  void showFoldersAndItemPropertiesReachTheWindowFactory();
  void refusedCallsAnswerInvalidArgsAndShowNothing();
  void aRequestNoWindowCouldShowAnswersFailed();
  void processWindowsReuseOnlyAnUnshownOrSameFolderWindow();

private:
  // A fresh service connection that owns the name, serving m_windows.
  [[nodiscard]] bool serve(const QString &connectionName);
  // Calls `method` from a separate client connection and waits for the reply.
  [[nodiscard]] QDBusMessage call(const QString &method, const QStringList &uris,
                                  const QString &token);

  // Declared before m_service, which borrows it.
  RecordingWindows m_windows;
  std::unique_ptr<FileManager1Service> m_service;
  QString m_serviceConnection;
  QTemporaryDir m_root;
};

void TestFileManager1Service::initTestCase() {
  QVERIFY2(QDBusConnection::sessionBus().isConnected(),
           "run this row under dbus-run-session");
  QVERIFY(m_root.isValid());
  QVERIFY(QDir(m_root.path()).mkdir(QStringLiteral("A")));
  QVERIFY(QDir(m_root.path()).mkdir(QStringLiteral("B")));
  QVERIFY(writeFile(m_root.filePath(QStringLiteral("A/one.txt"))));
  QVERIFY(writeFile(m_root.filePath(QStringLiteral("A/two.txt"))));
  QVERIFY(writeFile(m_root.filePath(QStringLiteral("B/three.txt"))));
}

void TestFileManager1Service::init() {
  m_windows.shown.clear();
  m_windows.accept = true;
}

void TestFileManager1Service::cleanup() {
  m_service.reset();
  if (!m_serviceConnection.isEmpty()) {
    QDBusConnection::disconnectFromBus(m_serviceConnection);
    m_serviceConnection.clear();
  }
}

bool TestFileManager1Service::serve(const QString &connectionName) {
  QDBusConnection connection =
      QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
  m_serviceConnection = connectionName;
  m_service = std::make_unique<FileManager1Service>(m_windows);
  if (!m_service->publish(connection)) {
    return false;
  }
  // The previous row's connection may still be releasing the name.
  return QTest::qWaitFor([&connection] {
    return connection.interface()->serviceOwner(FileManager1Service::serviceName()).value() ==
           connection.baseService();
  });
}

QDBusMessage TestFileManager1Service::call(const QString &method, const QStringList &uris,
                                           const QString &token) {
  QDBusConnection client = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                         QStringLiteral("file-manager1-client"));
  QDBusMessage message = QDBusMessage::createMethodCall(
      FileManager1Service::serviceName(), FileManager1Service::objectPath(),
      QStringLiteral("org.freedesktop.FileManager1"), method);
  // Never activate anything: only this process may answer.
  message.setAutoStartService(false);
  message << uris << token;
  QDBusPendingCallWatcher watcher(client.asyncCall(message));
  QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
  if (!watcher.isFinished() && !finished.wait(5000)) {
    return {};
  }
  return watcher.reply();
}

void TestFileManager1Service::servesTheFreedesktopNameAndQueuesTheNextProcess() {
  QVERIFY(serve(QStringLiteral("file-manager1-first")));

  // The interface is introspectable under its freedesktop.org names.
  QDBusConnection client = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                         QStringLiteral("file-manager1-client"));
  QDBusMessage introspect = QDBusMessage::createMethodCall(
      FileManager1Service::serviceName(), FileManager1Service::objectPath(),
      QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
  introspect.setAutoStartService(false);
  const QDBusMessage description = client.call(introspect, QDBus::BlockWithGui);
  QCOMPARE(description.type(), QDBusMessage::ReplyMessage);
  const QString xml = description.arguments().value(0).toString();
  for (const char *expected : {"org.freedesktop.FileManager1", "ShowFolders", "ShowItems",
                               "ShowItemProperties"}) {
    QVERIFY2(xml.contains(QLatin1String(expected)), expected);
  }

  // A second File Manager process waits in line instead of taking the name,
  // and serves as soon as the first one leaves.
  RecordingWindows secondWindows;
  FileManager1Service second(secondWindows);
  QDBusConnection secondConnection =
      QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("file-manager1-second"));
  QVERIFY(second.publish(secondConnection));
  QCOMPARE(client.interface()->serviceOwner(FileManager1Service::serviceName()).value(),
           QDBusConnection(m_serviceConnection).baseService());
  cleanup();
  QTRY_COMPARE(client.interface()->serviceOwner(FileManager1Service::serviceName()).value(),
               secondConnection.baseService());
  QVERIFY(call(QStringLiteral("ShowFolders"),
               {uriOf(m_root.filePath(QStringLiteral("A")))}, {})
              .type() == QDBusMessage::ReplyMessage);
  QCOMPARE(secondWindows.shown.size(), 1);
  QVERIFY(m_windows.shown.isEmpty());
  QDBusConnection::disconnectFromBus(QStringLiteral("file-manager1-second"));
}

void TestFileManager1Service::showItemsGroupsByFolderAndHandsTheTokenToTheFirstWindow() {
  QVERIFY(serve(QStringLiteral("file-manager1-items")));
  const QDBusMessage reply = call(QStringLiteral("ShowItems"),
                                  {uriOf(m_root.filePath(QStringLiteral("A/one.txt"))),
                                   uriOf(m_root.filePath(QStringLiteral("B/three.txt"))),
                                   uriOf(m_root.filePath(QStringLiteral("A/two.txt")))},
                                  QStringLiteral("activation-token-1"));
  QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
  QCOMPARE(m_windows.shown.size(), 2);
  QCOMPARE(m_windows.shown.at(0).request,
           (RevealRequest{canonical(m_root.filePath(QStringLiteral("A"))),
                          {QStringLiteral("one.txt"), QStringLiteral("two.txt")},
                          {}}));
  QCOMPARE(m_windows.shown.at(0).token, QStringLiteral("activation-token-1"));
  QCOMPARE(m_windows.shown.at(1).request,
           (RevealRequest{canonical(m_root.filePath(QStringLiteral("B"))),
                          {QStringLiteral("three.txt")},
                          {}}));
  // A token is single-use: the second window must not try to take focus.
  QCOMPARE(m_windows.shown.at(1).token, QString());
}

void TestFileManager1Service::showFoldersAndItemPropertiesReachTheWindowFactory() {
  QVERIFY(serve(QStringLiteral("file-manager1-folders")));
  QCOMPARE(call(QStringLiteral("ShowFolders"), {uriOf(m_root.filePath(QStringLiteral("B")))},
                {})
               .type(),
           QDBusMessage::ReplyMessage);
  QCOMPARE(call(QStringLiteral("ShowItemProperties"),
                {uriOf(m_root.filePath(QStringLiteral("A/two.txt")))}, {})
               .type(),
           QDBusMessage::ReplyMessage);
  QCOMPARE(m_windows.shown.size(), 2);
  QCOMPARE(m_windows.shown.at(0).request,
           (RevealRequest{canonical(m_root.filePath(QStringLiteral("B"))), {}, {}}));
  QCOMPARE(m_windows.shown.at(1).request,
           (RevealRequest{canonical(m_root.filePath(QStringLiteral("A"))),
                          {QStringLiteral("two.txt")},
                          QStringLiteral("file.properties")}));
}

void TestFileManager1Service::refusedCallsAnswerInvalidArgsAndShowNothing() {
  QVERIFY(serve(QStringLiteral("file-manager1-refusals")));
  const QString valid = uriOf(m_root.filePath(QStringLiteral("A/one.txt")));
  const QList<std::pair<QString, QStringList>> refused{
      {QStringLiteral("ShowItems"), {QStringLiteral("https://example.com/one.txt")}},
      {QStringLiteral("ShowItems"), {valid, uriOf(m_root.filePath(QStringLiteral("gone.txt")))}},
      {QStringLiteral("ShowFolders"), {valid}},
      {QStringLiteral("ShowItemProperties"), {QStringLiteral("file://otherhost/etc/passwd")}},
  };
  for (const auto &[method, uris] : refused) {
    const QDBusMessage reply = call(method, uris, {});
    QCOMPARE(reply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(reply.errorName(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
    QVERIFY(!reply.errorMessage().isEmpty());
  }
  QVERIFY(m_windows.shown.isEmpty());
}

void TestFileManager1Service::aRequestNoWindowCouldShowAnswersFailed() {
  m_windows.accept = false;
  QVERIFY(serve(QStringLiteral("file-manager1-failed")));
  const QDBusMessage reply =
      call(QStringLiteral("ShowFolders"), {uriOf(m_root.filePath(QStringLiteral("A")))}, {});
  QCOMPARE(reply.type(), QDBusMessage::ErrorMessage);
  QCOMPARE(reply.errorName(), QStringLiteral("org.freedesktop.DBus.Error.Failed"));
  QCOMPARE(m_windows.shown.size(), 1);
}

void TestFileManager1Service::processWindowsReuseOnlyAnUnshownOrSameFolderWindow() {
  const QString a = canonical(m_root.filePath(QStringLiteral("A")));
  const QString b = canonical(m_root.filePath(QStringLiteral("B")));
  NavigationController navigation(std::make_unique<LocalDirectoryLister>(),
                                  std::make_unique<Test::FakeFileLauncher>());
  navigation.navigateTo(a);
  QCOMPARE(navigation.currentPath(), a);

  QWindow window;
  auto *reveal = new FakeEntryReveal(&window);
  reveal->setObjectName(QStringLiteral("entryReveal"));
  struct Started final {
    QString program;
    QStringList arguments;
    QString token;
  };
  QList<Started> started;
  const QString program = QStringLiteral("/opt/test/qindaqt-file-manager");
  ProcessRevealWindows windows(
      window, navigation, program,
      [&started](const QString &name, const QStringList &arguments, const QString &token) {
        started.append({name, arguments, token});
        return true;
      });

  // A window that has shown nothing yet (a --service start) takes any folder.
  QVERIFY(!window.isVisible());
  QVERIFY(windows.show({b, {QStringLiteral("three.txt")}, {}}, {}));
  QCOMPARE(reveal->calls, (QList<RevealRequest>{{b, {QStringLiteral("three.txt")}, {}}}));
  QVERIFY(window.isVisible());
  QVERIFY(started.isEmpty());

  // A shown window is reused for the folder it shows (the navigation is at A).
  const QString info = QStringLiteral("file.properties");
  QVERIFY(windows.show({a, {QStringLiteral("one.txt")}, info}, {}));
  QCOMPARE(reveal->calls.size(), 2);
  QCOMPARE(reveal->calls.constLast(), (RevealRequest{a, {QStringLiteral("one.txt")}, info}));
  QVERIFY(started.isEmpty());

  // Any other folder is a new File Manager process on the reveal command line.
  const RevealRequest elsewhere{b, {QStringLiteral("-dash.txt")}, info};
  QVERIFY(windows.show(elsewhere, QStringLiteral("activation-token-2")));
  QCOMPARE(reveal->calls.size(), 2);
  QCOMPARE(started.size(), 1);
  QCOMPARE(started.constFirst().program, program);
  QCOMPARE(started.constFirst().arguments,
           (QStringList{QStringLiteral("--select=-dash.txt"),
                        QStringLiteral("--action=file.properties"), b}));
  QCOMPARE(started.constFirst().arguments,
           QindaQt::Apps::FileManager::Desktop::FileBoundary::revealArguments(elsewhere));
  QCOMPARE(started.constFirst().token, QStringLiteral("activation-token-2"));

  // Without the QML reveal object nothing is shown.
  QWindow bare;
  ProcessRevealWindows bareWindows(bare, navigation, program,
                                   [](const QString &, const QStringList &, const QString &) {
                                     return true;
                                   });
  QVERIFY(!bareWindows.show({a, {}, {}}, {}));
  QVERIFY(!bare.isVisible());
}

QTEST_MAIN(TestFileManager1Service)
#include "tst_file_manager1_service.moc"
