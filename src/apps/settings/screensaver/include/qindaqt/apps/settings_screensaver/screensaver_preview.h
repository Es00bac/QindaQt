// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <memory>

class QQuickWindow;
class QScreen;

namespace QindaQt::Session::DesktopControls {
class ScreensaverCatalog;
}

namespace QindaQt::Apps::SettingsScreensaver {

// Preview boundary for the Screen saver route (ADR-0259). Discovered savers
// use their catalog program and arguments; "blank" uses an input-dismissed
// black window. A preview never invokes the lock screen; it temporarily holds
// the standard idle-lock inhibitor while its surface is present.
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
  void failed(const QString &message);
};

// GUI-thread presentation seam for Settings-owned blank preview windows.
// ProcessScreensaverPreview owns the environment passed to it. Screen pointers
// returned by screens() are borrowed and used only during that show attempt;
// each QQuickWindow remains owned by the preview. showWindow() maps the window
// and requests activation when requested, returning false with a reason if
// the window cannot be shown. isActive() confirms keyboard focus
// asynchronously. The production implementation uses Qt's current screens and
// QWindow state; tests inject deterministic screen, show, and activation
// outcomes. This injection seam belongs to this module and is not a stable
// compatibility contract for other modules.
class ScreensaverPreviewEnvironment {
public:
  virtual ~ScreensaverPreviewEnvironment() = default;
  [[nodiscard]] virtual QList<QScreen *> screens() const = 0;
  [[nodiscard]] virtual bool showWindow(QQuickWindow &window, QScreen *screen,
                                        bool requestActivation,
                                        QString *error) = 0;
  [[nodiscard]] virtual bool isActive(const QQuickWindow &window) const = 0;
  [[nodiscard]] virtual int activationTimeoutMilliseconds() const noexcept = 0;
  [[nodiscard]] virtual int maximumDurationMilliseconds() const noexcept = 0;
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
  // Injectable presentation seam for display/focus failure coverage. The
  // preview takes ownership; GUI-thread-only calls and borrowed screen/window
  // lifetimes follow ScreensaverPreviewEnvironment's contract.
  ProcessScreensaverPreview(
      const QindaQt::Session::DesktopControls::ScreensaverCatalog &catalog,
      ProgramResolver programResolver,
      std::unique_ptr<ScreensaverPreviewEnvironment> environment,
      QObject *parent = nullptr);
  ~ProcessScreensaverPreview() override;

  [[nodiscard]] Kind kindFor(const QString &token) const override;
  [[nodiscard]] bool start(const QString &token, QString *error) override;
  [[nodiscard]] bool running() const override;

private:
  class BlankPreview;

  const QindaQt::Session::DesktopControls::ScreensaverCatalog &m_catalog;
  ProgramResolver m_programResolver;
  std::unique_ptr<ScreensaverPreviewEnvironment> m_environment;
  QProcess m_process;
  std::unique_ptr<BlankPreview> m_blankPreview;
  class ScreensaverPreviewIdleLockInhibitor;
  std::unique_ptr<ScreensaverPreviewIdleLockInhibitor> m_idleLockInhibitor;
  QTimer m_previewDeadlineTimer;
  QTimer m_processKillTimer;
  bool m_starting = false;
  bool m_stopRequested = false;
};

} // namespace QindaQt::Apps::SettingsScreensaver
