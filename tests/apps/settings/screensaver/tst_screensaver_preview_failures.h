// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "screensaver_preview_test_support.h"

#include <QObject>

class ScreensaverPreviewFailureTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void blankHasAnAutomaticEnd();
  void blankActivationDenialIsReportedAndClosesIt();
  void partialDisplayShowFailureClosesOpenedWindows();
  void missingLockAuthorityRefusesPreview();
  void startedSaverNonzeroExitIsReportedAndReleasesInhibit();
  void startedSaverCrashIsReportedAndReleasesInhibit();
  void settingsQuitReleasesInhibit();

private:
  QDBusConnection m_bus{QDBusConnection::sessionBus()};
  ScreensaverPreviewTestSupport::FakeScreenSaver m_screenSaver;
  ScreensaverPreviewTestSupport::FakeCatalog m_catalog;
};
