// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace QindaQt::Session::DesktopControls {
class ScreensaverCatalog;
}

namespace QindaQt::Apps::SettingsScreensaver {

// Preview boundary for the Screen saver route (ADR-0259). Discovered savers
// use their catalog program and arguments; "blank" uses an input-dismissed
// black window. A preview never invokes the lock screen or changes idle policy.
class ScreensaverPreview : public QObject {
  Q_OBJECT

public:
  // What previewing a given saver token means.
  enum class Kind {
    Unavailable,  // "none" or an unknown token
    BlackWindow,  // Settings-owned full-screen black window for "blank"
    SaverProgram, // the catalog program and arguments used by the idle path
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
  using ProgramResolver = std::function<QString(const QString &catalogProgram)>;

  explicit ProcessScreensaverPreview(
      const QindaQt::Session::DesktopControls::ScreensaverCatalog &catalog,
      QObject *parent = nullptr);
  // Test seam. The resolver is copied and called on the GUI thread only for a
  // discovered catalog program; return an executable or an empty string to
  // report a start failure.
  ProcessScreensaverPreview(
      const QindaQt::Session::DesktopControls::ScreensaverCatalog &catalog,
      ProgramResolver programResolver, QObject *parent = nullptr);
  ~ProcessScreensaverPreview() override;

  [[nodiscard]] Kind kindFor(const QString &token) const override;
  [[nodiscard]] bool start(const QString &token, QString *error) override;
  [[nodiscard]] bool running() const override;

private:
  class BlankPreview;

  const QindaQt::Session::DesktopControls::ScreensaverCatalog &m_catalog;
  ProgramResolver m_programResolver;
  QProcess m_process;
  std::unique_ptr<BlankPreview> m_blankPreview;
};

} // namespace QindaQt::Apps::SettingsScreensaver
