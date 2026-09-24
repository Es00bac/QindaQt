// SPDX-License-Identifier: GPL-3.0-or-later

#include "tst_screensaver_preview_failures.h"

#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace ScreensaverPreviewTestSupport;
using QindaQt::Apps::SettingsScreensaver::ProcessScreensaverPreview;
using QindaQt::Apps::SettingsScreensaver::ScreensaverPreview;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

namespace {

QString makeCrashingSaver(QTemporaryDir &directory,
                          const QString &startedPath) {
  QString quotedPath = startedPath;
  quotedPath.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  const QByteArray script = "#!/bin/sh\nprintf started > '" +
                            quotedPath.toUtf8() + "'\nkill -SEGV $$\n";
  const QString program = directory.filePath(QStringLiteral("crashing-saver"));
  QFile file(program);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return {};
  }
  if (file.write(script) != script.size()) {
    return {};
  }
  file.close();
  if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                           QFile::ExeOwner)) {
    return {};
  }
  return program;
}

} // namespace

void ScreensaverPreviewFailureTest::initTestCase() {
  QVERIFY(m_bus.isConnected());
  QVERIFY(m_bus.registerService(
      QString::fromLatin1(ScreensaverPreviewTestSupport::kScreenSaverService)));
  QVERIFY(m_bus.registerObject(
      QString::fromLatin1(ScreensaverPreviewTestSupport::kScreenSaverPath),
      &m_screenSaver, QDBusConnection::ExportAllSlots));
}

void ScreensaverPreviewFailureTest::cleanupTestCase() {
  m_bus.unregisterObject(
      QString::fromLatin1(ScreensaverPreviewTestSupport::kScreenSaverPath));
  m_bus.unregisterService(
      QString::fromLatin1(ScreensaverPreviewTestSupport::kScreenSaverService));
}

void ScreensaverPreviewFailureTest::init() { m_screenSaver.reset(); }

void ScreensaverPreviewFailureTest::blankHasAnAutomaticEnd() {
  auto environment =
      std::make_unique<TestScreensaverEnvironment>(QGuiApplication::screens());
  environment->maximumDuration = 10;
  QStringList resolvedPrograms;
  QString error;
  auto preview = startBlankPreview(m_catalog, &resolvedPrograms, &error,
                                   std::move(environment));
  QVERIFY2(preview != nullptr, qPrintable(error));
  QSignalSpy finishedSpy(preview.get(), &ScreensaverPreview::finished);
  QSignalSpy failedSpy(preview.get(), &ScreensaverPreview::failed);

  QTRY_VERIFY_WITH_TIMEOUT(!preview->running(), 1000);
  QCOMPARE(finishedSpy.count(), 1);
  QCOMPARE(failedSpy.count(), 0);
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}

void ScreensaverPreviewFailureTest::
    blankActivationDenialIsReportedAndClosesIt() {
  auto environment =
      std::make_unique<TestScreensaverEnvironment>(QGuiApplication::screens());
  environment->active = false;
  environment->activationTimeout = 10;
  QStringList resolvedPrograms;
  QString error;
  ProcessScreensaverPreview preview(
      m_catalog,
      [&resolvedPrograms](const QString &program) {
        resolvedPrograms.append(program);
        return QStringLiteral("/bin/true");
      },
      std::move(environment));
  QSignalSpy finishedSpy(&preview, &ScreensaverPreview::finished);
  QSignalSpy failedSpy(&preview, &ScreensaverPreview::failed);

  QVERIFY2(preview.start(ScreensaverPreferences::blankToken(), &error),
           qPrintable(error));
  QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(!preview.running(), 1000);
  QCOMPARE(finishedSpy.count(), 1);
  QVERIFY(failedSpy.constFirst().constFirst().toString().contains(
      QStringLiteral("keyboard focus")));
  QVERIFY(blankPreviewWindows().isEmpty());
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}

void ScreensaverPreviewFailureTest::
    partialDisplayShowFailureClosesOpenedWindows() {
  QList<QScreen *> screens = QGuiApplication::screens();
  QVERIFY(!screens.isEmpty());
  screens.append(screens.constFirst());
  auto environment =
      std::make_unique<TestScreensaverEnvironment>(std::move(screens));
  int showCount = 0;
  environment->m_presenter = [&showCount](QQuickWindow &window, QScreen *screen,
                                          bool requestActivation,
                                          QString *error) {
    ++showCount;
    if (showCount == 2) {
      if (error != nullptr) {
        *error = QStringLiteral("simulated display refusal");
      }
      return false;
    }
    window.setScreen(screen);
    window.showFullScreen();
    if (requestActivation) {
      window.requestActivate();
    }
    return window.isVisible();
  };
  QStringList resolvedPrograms;
  QString error;
  ProcessScreensaverPreview preview(
      m_catalog,
      [&resolvedPrograms](const QString &program) {
        resolvedPrograms.append(program);
        return QStringLiteral("/bin/true");
      },
      std::move(environment));

  QVERIFY(!preview.start(ScreensaverPreferences::blankToken(), &error));
  QCOMPARE(showCount, 2);
  QVERIFY(error.contains(QStringLiteral("every display")));
  QVERIFY(error.contains(QStringLiteral("simulated display refusal")));
  QVERIFY(!preview.running());
  QVERIFY(blankPreviewWindows().isEmpty());
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}

void ScreensaverPreviewFailureTest::missingLockAuthorityRefusesPreview() {
  QVERIFY(m_bus.unregisterService(
      QString::fromLatin1(ScreensaverPreviewTestSupport::kScreenSaverService)));
  auto environment =
      std::make_unique<TestScreensaverEnvironment>(QGuiApplication::screens());
  QString error;
  ProcessScreensaverPreview preview(
      m_catalog, [](const QString &) { return QStringLiteral("/bin/true"); },
      std::move(environment));

  QVERIFY(!preview.start(ScreensaverPreferences::blankToken(), &error));
  QVERIFY(error.contains(QStringLiteral("could not be inhibited")));
  QVERIFY(!preview.running());
  QVERIFY(blankPreviewWindows().isEmpty());
  QCOMPARE(m_screenSaver.inhibitCalls, 0);
  QVERIFY(m_bus.registerService(
      QString::fromLatin1(ScreensaverPreviewTestSupport::kScreenSaverService)));
}

void ScreensaverPreviewFailureTest::
    startedSaverNonzeroExitIsReportedAndReleasesInhibit() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString startedPath = directory.filePath(QStringLiteral("started"));
  const QString program = makeNonzeroSaver(directory, startedPath);
  QVERIFY(!program.isEmpty());

  class BrokenCatalog final : public ScreensaverCatalog {
  public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override {
      return {{QStringLiteral("broken-saver"),
               QStringLiteral("Broken Saver"),
               {},
               {},
               {QStringLiteral("--screensaver")},
               false}};
    }
  } brokenCatalog;
  ProcessScreensaverPreview preview(
      brokenCatalog, [&program](const QString &) { return program; });
  QSignalSpy finishedSpy(&preview, &ScreensaverPreview::finished);
  QSignalSpy failedSpy(&preview, &ScreensaverPreview::failed);
  QString error;

  QVERIFY2(preview.start(QStringLiteral("broken-saver"), &error),
           qPrintable(error));
  QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
  QCOMPARE(failedSpy.count(), 1);
  QVERIFY(QFile::exists(startedPath));
  const QString message = failedSpy.constFirst().constFirst().toString();
  QVERIFY(message.contains(QStringLiteral("23")));
  QVERIFY(message.contains(QStringLiteral("normally")));
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}

void ScreensaverPreviewFailureTest::
    startedSaverCrashIsReportedAndReleasesInhibit() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString startedPath = directory.filePath(QStringLiteral("started"));
  const QString program = makeCrashingSaver(directory, startedPath);
  QVERIFY(!program.isEmpty());

  class CrashingCatalog final : public ScreensaverCatalog {
  public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override {
      return {{QStringLiteral("crashing-saver"),
               QStringLiteral("Crashing Saver"),
               {},
               {},
               {},
               false}};
    }
  } crashingCatalog;
  ProcessScreensaverPreview preview(
      crashingCatalog, [&program](const QString &) { return program; });
  QSignalSpy finishedSpy(&preview, &ScreensaverPreview::finished);
  QSignalSpy failedSpy(&preview, &ScreensaverPreview::failed);
  QString error;

  QVERIFY2(preview.start(QStringLiteral("crashing-saver"), &error),
           qPrintable(error));
  QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
  QCOMPARE(failedSpy.count(), 1);
  QVERIFY(QFile::exists(startedPath));
  const QString message = failedSpy.constFirst().constFirst().toString();
  QVERIFY(message.contains(QStringLiteral("crashed")));
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
}

void ScreensaverPreviewFailureTest::settingsQuitReleasesInhibit() {
  class SlowCatalog final : public ScreensaverCatalog {
  public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override {
      return {{QStringLiteral("sleep"),
               QStringLiteral("Sleeper"),
               {},
               {},
               {QStringLiteral("5")},
               false}};
    }
  } slowCatalog;

  auto preview = std::make_unique<ProcessScreensaverPreview>(slowCatalog);
  QString error;
  QVERIFY2(preview->start(QStringLiteral("sleep"), &error), qPrintable(error));
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.activeCookies.size(), 1);

  preview.reset();
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}
