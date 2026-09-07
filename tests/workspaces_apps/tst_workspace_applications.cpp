// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/workspaces_apps/desktop_applications.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::WorkspacesApps;
class ApplicationsTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    const QString root = QString::fromUtf8(qgetenv("XDG_DATA_HOME"));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/applications")));
    QFile helper(root + QStringLiteral("/capture-arguments"));
    QVERIFY(helper.open(QIODevice::WriteOnly));
    const QByteArray program = "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"" +
                               root.toUtf8() + "/arguments\"\n";
    QCOMPARE(helper.write(program), qint64(program.size()));
    helper.close();
    QVERIFY(helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                                  QFile::ExeOwner));
    auto desktop = [&](const QString &id, const QString &exec) {
      QFile file(root + QStringLiteral("/applications/") + id +
                 QStringLiteral(".desktop"));
      if (!file.open(QIODevice::WriteOnly))
        return false;
      const auto bytes =
          QStringLiteral("[Desktop Entry]\nType=Application\nName=Workspace "
                         "Test\nExec=%1\nTerminal=false\nStartupNotify=false\n")
              .arg(exec)
              .toUtf8();
      return file.write(bytes) == bytes.size();
    };
    QVERIFY(desktop(QStringLiteral("workspace-test"),
                    QStringLiteral("\"%1/capture-arguments\" %%U")
                        .arg(root)
                        .replace(QStringLiteral("%%U"), QStringLiteral("%U"))));
    QVERIFY(desktop(QStringLiteral("workspace-broken"),
                    QStringLiteral("/nonexistent-qindaqt-test-command")));
    QProcess cache;
    cache.start(QStringLiteral("kbuildsycoca6"),
                {QStringLiteral("--noincremental")});
    QVERIFY(cache.waitForFinished(10000));
    QCOMPARE(cache.exitCode(), 0);
  }
  void missingApplicationIsReported() {
    DesktopApplications applications;
    QVERIFY(!applications.find(QStringLiteral("does-not-exist")));
    QVERIFY(!applications.find(QStringLiteral("../workspace-test.desktop")));
    QString error;
    QVERIFY(
        !applications.launch(QStringLiteral("does-not-exist"), {}, {}, &error));
    QVERIFY(error.contains(QStringLiteral("not installed")));
  }
  void launchesDesktopEntryWithUrl() {
    DesktopApplications applications;
    const auto app = applications.find(QStringLiteral("workspace-test"));
    QVERIFY(app.has_value());
    QCOMPARE(app->name, QStringLiteral("Workspace Test"));
    QSignalSpy finished(&applications, &DesktopApplications::launchFinished);
    QString error;
    QVERIFY2(applications.launch(
                 QStringLiteral("workspace-test"),
                 {QStringLiteral("file:///tmp/a%20document.txt")}, {}, &error),
             qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QVERIFY2(finished.first().at(1).toBool(),
             qPrintable(finished.first().at(2).toString()));
    QFile arguments(QString::fromUtf8(qgetenv("XDG_DATA_HOME")) +
                    QStringLiteral("/arguments"));
    QTRY_VERIFY(arguments.exists());
    QVERIFY(arguments.open(QIODevice::ReadOnly));
    const auto actual = arguments.readAll();
    QVERIFY2(actual.contains("file:///tmp/a%20document.txt") ||
                 actual.contains("/tmp/a document.txt"),
             actual.constData());
  }
  void failedExecutableReportsAsynchronousFailure() {
    DesktopApplications applications;
    QSignalSpy finished(&applications, &DesktopApplications::launchFinished);
    QVERIFY(applications.launch(QStringLiteral("workspace-broken"), {}));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QVERIFY(!finished.first().at(1).toBool());
    QVERIFY(!finished.first().at(2).toString().isEmpty());
  }
  void invalidSavedUrlDoesNotLaunch() {
    DesktopApplications applications;
    QSignalSpy finished(&applications, &DesktopApplications::launchFinished);
    QString error;
    QVERIFY(!applications.launch(QStringLiteral("workspace-test"),
                                 {QStringLiteral("relative.txt")}, {}, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(finished.count(), 0);
  }
};
int main(int argc, char **argv) {
  QTemporaryDir directory;
  if (!directory.isValid())
    return 1;
  qputenv("XDG_DATA_HOME", directory.path().toUtf8());
  qputenv("XDG_DATA_DIRS",
          (directory.path() + QStringLiteral("/empty")).toUtf8());
  qputenv("XDG_CACHE_HOME",
          (directory.path() + QStringLiteral("/cache")).toUtf8());
  QGuiApplication application(argc, argv);
  ApplicationsTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "tst_workspace_applications.moc"
