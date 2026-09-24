// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>

#include <QCursor>
#include <QDBusConnection>
#include <QFile>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>
#include <QTemporaryDir>

#include <functional>
#include <memory>

namespace ScreensaverPreviewTestSupport {

using QindaQt::Apps::SettingsScreensaver::ProcessScreensaverPreview;
using QindaQt::Apps::SettingsScreensaver::ScreensaverPreview;
using QindaQt::Apps::SettingsScreensaver::ScreensaverPreviewEnvironment;
using QindaQt::Session::DesktopControls::ScreensaverCatalog;
using QindaQt::Session::DesktopControls::ScreensaverCatalogEntry;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

constexpr auto kScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto kScreenSaverPath = "/ScreenSaver";

class TestScreensaverEnvironment final : public ScreensaverPreviewEnvironment {
public:
  using Presenter =
      std::function<bool(QQuickWindow &, QScreen *, bool, QString *)>;

  explicit TestScreensaverEnvironment(QList<QScreen *> screens)
      : m_screens(std::move(screens)) {}

  [[nodiscard]] QList<QScreen *> screens() const override { return m_screens; }

  [[nodiscard]] bool showWindow(QQuickWindow &window, QScreen *screen,
                                bool requestActivation,
                                QString *error) override {
    if (m_presenter) {
      return m_presenter(window, screen, requestActivation, error);
    }
    window.setScreen(screen);
    window.showFullScreen();
    if (requestActivation) {
      window.requestActivate();
    }
    return window.isVisible();
  }

  [[nodiscard]] bool isActive(const QQuickWindow &) const override {
    return active;
  }
  [[nodiscard]] int activationTimeoutMilliseconds() const noexcept override {
    return activationTimeout;
  }
  [[nodiscard]] int maximumDurationMilliseconds() const noexcept override {
    return maximumDuration;
  }

  QList<QScreen *> m_screens;
  Presenter m_presenter;
  bool active = true;
  int activationTimeout = 1'000;
  int maximumDuration = 60'000;
};

class FakeScreenSaver final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

public:
  void reset() {
    inhibitCalls = 0;
    uninhibitCalls = 0;
    activeCookies.clear();
    lastApplication.clear();
    lastReason.clear();
    nextCookie = 41;
  }

public slots:
  uint Inhibit(const QString &application, const QString &reason) {
    ++inhibitCalls;
    lastApplication = application;
    lastReason = reason;
    const uint cookie = nextCookie++;
    activeCookies.insert(cookie);
    return cookie;
  }

  void UnInhibit(uint cookie) {
    ++uninhibitCalls;
    activeCookies.remove(cookie);
  }

public:
  int inhibitCalls = 0;
  int uninhibitCalls = 0;
  uint nextCookie = 41;
  QSet<uint> activeCookies;
  QString lastApplication;
  QString lastReason;
};

class FakeCatalog final : public ScreensaverCatalog {
public:
  [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override {
    return {
        {QStringLiteral("qinda-patrol"),
         QStringLiteral("Qinda Patrol"),
         {},
         {},
         {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")},
         true},
        {QStringLiteral("circuit-reef"),
         QStringLiteral("Circuit Reef"),
         {},
         {},
         {QStringLiteral("--screensaver"), QStringLiteral("--private")},
         true},
        {QStringLiteral("prism-brawl"),
         QStringLiteral("Prism Brawl"),
         {},
         {},
         {QStringLiteral("--screensaver"), QStringLiteral("--mute")},
         false},
        {QStringLiteral("new-house-saver"),
         QStringLiteral("New Saver"),
         {},
         {},
         {QStringLiteral("--screensaver")},
         false},
    };
  }
};

inline QString makeArgumentRecorder(QTemporaryDir &directory,
                                    const QString &argumentsPath) {
  QString quotedPath = argumentsPath;
  quotedPath.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  const QByteArray script =
      "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + quotedPath.toUtf8() + "'\n";
  const QString program = directory.filePath(QStringLiteral("fake-saver"));
  QFile file(program);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  if (file.write(script) != script.size())
    return {};
  file.close();
  if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                           QFile::ExeOwner)) {
    return {};
  }
  return program;
}

inline QString makeNonzeroSaver(QTemporaryDir &directory,
                                const QString &startedPath) {
  QString quotedPath = startedPath;
  quotedPath.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  const QByteArray script =
      "#!/bin/sh\nprintf started > '" + quotedPath.toUtf8() + "'\nexit 23\n";
  const QString program = directory.filePath(QStringLiteral("broken-saver"));
  QFile file(program);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  if (file.write(script) != script.size())
    return {};
  file.close();
  if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                           QFile::ExeOwner)) {
    return {};
  }
  return program;
}

inline QStringList recordedArguments(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(file.readAll())
      .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

inline QQuickWindow *blankPreviewWindow() {
  for (QWindow *window : QGuiApplication::topLevelWindows()) {
    if (window->objectName() ==
        QStringLiteral("qindaqtBlankScreensaverPreview")) {
      return qobject_cast<QQuickWindow *>(window);
    }
  }
  return nullptr;
}

inline QList<QQuickWindow *> blankPreviewWindows() {
  QList<QQuickWindow *> windows;
  for (QWindow *window : QGuiApplication::topLevelWindows()) {
    if (window->objectName() ==
        QStringLiteral("qindaqtBlankScreensaverPreview")) {
      if (auto *quickWindow = qobject_cast<QQuickWindow *>(window)) {
        windows.append(quickWindow);
      }
    }
  }
  return windows;
}

inline std::unique_ptr<ProcessScreensaverPreview>
startBlankPreview(const FakeCatalog &catalog, QStringList *resolvedPrograms,
                  QString *error,
                  std::unique_ptr<ScreensaverPreviewEnvironment> environment =
                      std::make_unique<TestScreensaverEnvironment>(
                          QGuiApplication::screens())) {
  auto preview = std::make_unique<ProcessScreensaverPreview>(
      catalog,
      [resolvedPrograms](const QString &program) {
        resolvedPrograms->append(program);
        return QStringLiteral("/bin/true");
      },
      std::move(environment));
  if (!preview->start(ScreensaverPreferences::blankToken(), error))
    return {};
  return preview;
}

} // namespace ScreensaverPreviewTestSupport
