// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>

#include "screensaver_preview_test_support.h"
#include "tst_screensaver_preview_failures.h"

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace ScreensaverPreviewTestSupport;
using QindaQt::Apps::SettingsScreensaver::ProcessScreensaverPreview;
using QindaQt::Apps::SettingsScreensaver::ScreensaverPreview;
using QindaQt::Session::DesktopControls::ScreensaverCatalog;
using QindaQt::Session::DesktopControls::ScreensaverCatalogEntry;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

class ScreensaverPreviewTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void kindForNothingChosenIsUnavailable();
  void kindForUnknownTokenIsUnavailable();
  void blankUsesTheBlackWindowPath();
  void everyDiscoveredSaverUsesItsCatalogProgramAndArguments();
  void emptyProgramResolutionReportsFailure();
  void startRefusesWhenThereIsNothingToPreview();
  void blankClosesOnAnyKey();
  void blankClosesOnEscape();
  void blankClosesOnClick();
  void blankClosesOnPointerMotion();
  void previewsNeverStack();

private:
  QDBusConnection m_bus{QDBusConnection::sessionBus()};
  FakeScreenSaver m_screenSaver;
  FakeCatalog m_catalog;
};

void ScreensaverPreviewTest::initTestCase() {
  QVERIFY(m_bus.isConnected());
  QVERIFY(m_bus.registerService(QString::fromLatin1(kScreenSaverService)));
  QVERIFY(m_bus.registerObject(QString::fromLatin1(kScreenSaverPath),
                               &m_screenSaver,
                               QDBusConnection::ExportAllSlots));
}

void ScreensaverPreviewTest::cleanupTestCase() {
  m_bus.unregisterObject(QString::fromLatin1(kScreenSaverPath));
  m_bus.unregisterService(QString::fromLatin1(kScreenSaverService));
}

void ScreensaverPreviewTest::init() { m_screenSaver.reset(); }

void ScreensaverPreviewTest::kindForNothingChosenIsUnavailable() {
  ProcessScreensaverPreview preview(m_catalog);
  QCOMPARE(preview.kindFor(ScreensaverPreferences::noneToken()),
           ScreensaverPreview::Kind::Unavailable);
  QCOMPARE(preview.kindFor(QString()), ScreensaverPreview::Kind::Unavailable);
}

void ScreensaverPreviewTest::kindForUnknownTokenIsUnavailable() {
  ProcessScreensaverPreview preview(m_catalog);
  // A hand-edited token must never become a program name here either.
  QCOMPARE(preview.kindFor(QStringLiteral("xscreensaver")),
           ScreensaverPreview::Kind::Unavailable);
}

void ScreensaverPreviewTest::blankUsesTheBlackWindowPath() {
  QStringList resolvedPrograms;
  QString error;
  auto preview = startBlankPreview(m_catalog, &resolvedPrograms, &error);
  QVERIFY2(preview != nullptr, qPrintable(error));
  QCOMPARE(preview->kindFor(ScreensaverPreferences::blankToken()),
           ScreensaverPreview::Kind::BlackWindow);
  QVERIFY(resolvedPrograms.isEmpty());
  QVERIFY(preview->running());
  QVERIFY(!preview->start(ScreensaverPreferences::blankToken(), &error));
  QVERIFY(!error.isEmpty());
  QCOMPARE(m_screenSaver.inhibitCalls, 1);
  QCOMPARE(m_screenSaver.activeCookies.size(), 1);
  QCOMPARE(m_screenSaver.lastApplication,
           QStringLiteral("org.qindaqt.Settings"));
  QVERIFY(m_screenSaver.lastReason.contains(QStringLiteral("Preview")));

  QQuickWindow *window = blankPreviewWindow();
  QVERIFY(window != nullptr);
  QVERIFY(window->isVisible());
  QCOMPARE(window->color(), QColor(Qt::black));
  QCOMPARE(window->windowState(), Qt::WindowFullScreen);
}

void ScreensaverPreviewTest::
    everyDiscoveredSaverUsesItsCatalogProgramAndArguments() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString argumentsPath =
      directory.filePath(QStringLiteral("arguments.txt"));
  const QString recorder = makeArgumentRecorder(directory, argumentsPath);
  QVERIFY(!recorder.isEmpty());

  QStringList resolvedPrograms;
  ProcessScreensaverPreview preview(
      m_catalog, [&resolvedPrograms, &recorder](const QString &program) {
        resolvedPrograms.append(program);
        return recorder;
      });
  QSignalSpy finishedSpy(&preview, &ScreensaverPreview::finished);
  const QList<ScreensaverCatalogEntry> entries = m_catalog.entries();
  for (qsizetype index = 0; index < entries.size(); ++index) {
    const ScreensaverCatalogEntry &entry = entries.at(index);
    QCOMPARE(preview.kindFor(entry.token),
             ScreensaverPreview::Kind::SaverProgram);
    QFile::remove(argumentsPath);
    QString error;
    QVERIFY2(preview.start(entry.token, &error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), int(index + 1), 3000);
    QCOMPARE(recordedArguments(argumentsPath), entry.arguments);
  }

  QStringList expectedPrograms;
  for (const ScreensaverCatalogEntry &entry : entries) {
    expectedPrograms.append(entry.token);
  }
  QCOMPARE(resolvedPrograms, expectedPrograms);
  QVERIFY(!resolvedPrograms.contains(QStringLiteral("kscreenlocker_greet")));
  QCOMPARE(m_screenSaver.inhibitCalls, int(entries.size()));
  QCOMPARE(m_screenSaver.uninhibitCalls, int(entries.size()));
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}

void ScreensaverPreviewTest::emptyProgramResolutionReportsFailure() {
  ProcessScreensaverPreview preview(m_catalog,
                                    [](const QString &) { return QString{}; });
  QString error;
  QVERIFY(!preview.start(QStringLiteral("prism-brawl"), &error));
  QVERIFY(!error.isEmpty());
  QVERIFY(!preview.running());
}

void ScreensaverPreviewTest::startRefusesWhenThereIsNothingToPreview() {
  ProcessScreensaverPreview preview(m_catalog);
  QString error;
  QVERIFY(!preview.start(ScreensaverPreferences::noneToken(), &error));
  QVERIFY(!error.isEmpty());
  QVERIFY(!preview.running());
}

void ScreensaverPreviewTest::blankClosesOnAnyKey() {
  QStringList resolvedPrograms;
  QString error;
  auto preview = startBlankPreview(m_catalog, &resolvedPrograms, &error);
  QVERIFY2(preview != nullptr, qPrintable(error));
  QQuickWindow *window = blankPreviewWindow();
  QVERIFY(window != nullptr);
  QSignalSpy finishedSpy(preview.get(), &ScreensaverPreview::finished);

  QTest::keyClick(window, Qt::Key_A);
  QTRY_VERIFY(!preview->running());
  QCOMPARE(finishedSpy.count(), 1);
  QCOMPARE(m_screenSaver.uninhibitCalls, 1);
  QVERIFY(m_screenSaver.activeCookies.isEmpty());
}

void ScreensaverPreviewTest::blankClosesOnEscape() {
  QStringList resolvedPrograms;
  QString error;
  auto preview = startBlankPreview(m_catalog, &resolvedPrograms, &error);
  QVERIFY2(preview != nullptr, qPrintable(error));
  QQuickWindow *window = blankPreviewWindow();
  QVERIFY(window != nullptr);
  QSignalSpy finishedSpy(preview.get(), &ScreensaverPreview::finished);

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(!preview->running());
  QCOMPARE(finishedSpy.count(), 1);
}

void ScreensaverPreviewTest::blankClosesOnClick() {
  QStringList resolvedPrograms;
  QString error;
  auto preview = startBlankPreview(m_catalog, &resolvedPrograms, &error);
  QVERIFY2(preview != nullptr, qPrintable(error));
  QQuickWindow *window = blankPreviewWindow();
  QVERIFY(window != nullptr);
  QSignalSpy finishedSpy(preview.get(), &ScreensaverPreview::finished);

  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                    QPoint(window->width() / 2, window->height() / 2));
  QTRY_VERIFY(!preview->running());
  QCOMPARE(finishedSpy.count(), 1);
}

void ScreensaverPreviewTest::blankClosesOnPointerMotion() {
  QStringList resolvedPrograms;
  QString error;
  auto preview = startBlankPreview(m_catalog, &resolvedPrograms, &error);
  QVERIFY2(preview != nullptr, qPrintable(error));
  QQuickWindow *window = blankPreviewWindow();
  QVERIFY(window != nullptr);
  QSignalSpy finishedSpy(preview.get(), &ScreensaverPreview::finished);
  QCoreApplication::processEvents();
  QVERIFY(preview->running());

  const QPoint firstTarget(20, 20);
  const QPoint secondTarget(40, 40);
  const QPoint target = window->mapToGlobal(firstTarget) == QCursor::pos()
                            ? secondTarget
                            : firstTarget;
  QTest::mouseMove(window, target);
  QTRY_VERIFY(!preview->running());
  QCOMPARE(finishedSpy.count(), 1);
}

void ScreensaverPreviewTest::previewsNeverStack() {
  // The catalog token names the program; `sleep` is a harmless installed
  // utility that holds the process boundary long enough to test admission.
  class SlowCatalog final : public ScreensaverCatalog {
  public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override {
      return {{QStringLiteral("sleep"),
               QStringLiteral("Sleeper"),
               {},
               {},
               {QStringLiteral("2")},
               false}};
    }
  } slowCatalog;

  ProcessScreensaverPreview preview(slowCatalog);
  QString error;
  QVERIFY2(preview.start(QStringLiteral("sleep"), &error), qPrintable(error));
  QVERIFY(preview.running());
  QVERIFY(!preview.start(QStringLiteral("sleep"), &error));
  QVERIFY(!error.isEmpty());
  QTRY_VERIFY_WITH_TIMEOUT(!preview.running(), 3000);
}

int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  ScreensaverPreviewTest previewTest;
  const int previewResult = QTest::qExec(&previewTest, argc, argv);
  ScreensaverPreviewFailureTest failureTest;
  return previewResult | QTest::qExec(&failureTest, argc, argv);
}

#include "tst_screensaver_preview.moc"
