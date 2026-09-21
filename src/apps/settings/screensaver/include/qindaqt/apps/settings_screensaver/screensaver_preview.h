// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace QindaQt::Session::DesktopControls {
class ScreensaverCatalog;
}

namespace QindaQt::Apps::SettingsScreensaver {

// Preview boundary for the Screen saver route (ADR-0226). A preview never
// locks the session: savers the locker's wallpaper plugin can draw (and the
// reserved "blank" choice) are shown through `kscreenlocker_greet --testing`,
// the documented way to exercise the lock screen without locking; a saver the
// greeter cannot draw runs as itself, exactly as it appears while the session
// is unlocked but idle, and exits on input like it does there.
class ScreensaverPreview : public QObject {
  Q_OBJECT

public:
  // What previewing a given saver token means.
  enum class Kind {
    Unavailable,   // "none", an unknown token, or a missing preview program
    TestingGreeter,// kscreenlocker_greet --testing, the lock-screen rendering
    SaverProgram,  // the saver binary itself, the unlocked-idle rendering
  };
  Q_ENUM(Kind)

  using QObject::QObject;
  ~ScreensaverPreview() override = default;

  ScreensaverPreview(const ScreensaverPreview &) = delete;
  ScreensaverPreview &operator=(const ScreensaverPreview &) = delete;

  [[nodiscard]] virtual Kind kindFor(const QString &token) const = 0;
  // Starts the preview; returns false with a translated reason on failure.
  // Starting while a preview runs is refused so instances never stack.
  [[nodiscard]] virtual bool start(const QString &token, QString *error) = 0;
  [[nodiscard]] virtual bool running() const = 0;

Q_SIGNALS:
  void finished();
};

class ProcessScreensaverPreview final : public ScreensaverPreview {
  Q_OBJECT

public:
  explicit ProcessScreensaverPreview(
      const QindaQt::Session::DesktopControls::ScreensaverCatalog &catalog,
      QObject *parent = nullptr);
  // Test seam: an explicit greeter path instead of the platform lookup.
  ProcessScreensaverPreview(
      const QindaQt::Session::DesktopControls::ScreensaverCatalog &catalog,
      QString greeterPath, QObject *parent = nullptr);
  ~ProcessScreensaverPreview() override;

  [[nodiscard]] Kind kindFor(const QString &token) const override;
  [[nodiscard]] bool start(const QString &token, QString *error) override;
  [[nodiscard]] bool running() const override;

  // The greeter is a libexec binary that is not on PATH; the lookup order is
  // PATH first, then the known install roots.
  [[nodiscard]] static QStringList greeterCandidates();
  [[nodiscard]] static QString resolveGreeter();

private:
  [[nodiscard]] QString greeter() const;

  const QindaQt::Session::DesktopControls::ScreensaverCatalog &m_catalog;
  QString m_greeterPath; // empty until resolved; explicit in tests
  QProcess m_process;
};

} // namespace QindaQt::Apps::SettingsScreensaver
