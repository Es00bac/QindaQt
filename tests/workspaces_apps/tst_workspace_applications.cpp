// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/workspaces_apps/desktop_applications.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::WorkspacesApps;
class ApplicationsTest final : public QObject {
  Q_OBJECT
private slots:
  void init() { QTest::failOnWarning(); }
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
    auto identityEntry = [&](const QString &id, const QString &name,
                             const QString &icon, const QString &startupClass) {
      QFile file(root + QStringLiteral("/applications/") + id
                 + QStringLiteral(".desktop"));
      if (!file.open(QIODevice::WriteOnly))
        return false;
      const auto bytes = QStringLiteral(
          "[Desktop Entry]\nType=Application\nName=%1\nIcon=%2\nExec=/bin/true\nStartupWMClass=%3\n")
          .arg(name, icon, startupClass).toUtf8();
      return file.write(bytes) == bytes.size();
    };
    QVERIFY(identityEntry(QStringLiteral("browser-variant"),
                          QStringLiteral("Browser Variant"),
                          QStringLiteral("browser-variant-icon"),
                          QStringLiteral("Vendor.Browser.Window")));
    QVERIFY(identityEntry(QStringLiteral("exact-browser"),
                          QStringLiteral("Exact Browser"),
                          QStringLiteral("exact-browser-icon"),
                          QStringLiteral("LegacyBrowser")));
    QVERIFY(identityEntry(QStringLiteral("shared-one"), QStringLiteral("Shared One"),
                          QStringLiteral("shared-one-icon"),
                          QStringLiteral("SharedWindowClass")));
    QVERIFY(identityEntry(QStringLiteral("shared-two"), QStringLiteral("Shared Two"),
                          QStringLiteral("shared-two-icon"),
                          QStringLiteral("SharedWindowClass")));
    QProcess cache;
    cache.start(QStringLiteral("kbuildsycoca6"),
                {QStringLiteral("--noincremental")});
    QVERIFY(cache.waitForFinished(10000));
    QCOMPARE(cache.exitCode(), 0);
  }
  void startupWmClassResolvesAUniqueWindowAlias() {
    DesktopApplications applications;
    const auto browser = applications.findForWindow(
        {}, QStringLiteral("vendor.browser.window"), {});
    QVERIFY(browser);
    QCOMPARE(browser->id, QStringLiteral("browser-variant"));
    QCOMPARE(browser->name, QStringLiteral("Browser Variant"));
    QCOMPARE(browser->iconName, QStringLiteral("browser-variant-icon"));
  }
  void exactIdPrecedesAliasesAndAmbiguousAliasesFailClosed() {
    DesktopApplications applications;
    const auto exact = applications.findForWindow(
        QStringLiteral("exact-browser"), {}, QStringLiteral("LegacyBrowser"));
    QVERIFY(exact);
    QCOMPARE(exact->id, QStringLiteral("exact-browser"));
    QVERIFY(!applications.findForWindow({}, {}, QStringLiteral("SharedWindowClass")));
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
    QTest::ignoreMessage(QtWarningMsg,
        "Failed to determine systemd version, falling back to extremely legacy forking mode.");
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
    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression(QStringLiteral(".*Could not find the program.*")));
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
