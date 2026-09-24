// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>

#include "screensaver_idle_lock_inhibitor_p.h"

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>

#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#include <optional>
#include <utility>
#include <vector>

using QindaQt::Session::DesktopControls::ScreensaverCatalog;
using QindaQt::Session::DesktopControls::ScreensaverCatalogEntry;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

namespace QindaQt::Apps::SettingsScreensaver {
namespace {

constexpr int kDefaultActivationTimeoutMilliseconds = 1'000;
constexpr int kMaximumPreviewDurationMilliseconds = 60'000;
constexpr int kProcessStopGraceMilliseconds = 500;

QString translated(const char *sourceText) {
  return QCoreApplication::translate("ProcessScreensaverPreview", sourceText);
}

int boundedPreviewDurationMilliseconds(
    const ScreensaverPreviewEnvironment &environment) {
  return qBound(1, environment.maximumDurationMilliseconds(),
                kMaximumPreviewDurationMilliseconds);
}

class QtScreensaverPreviewEnvironment final
    : public ScreensaverPreviewEnvironment {
public:
  [[nodiscard]] QList<QScreen *> screens() const override {
    return QGuiApplication::screens();
  }

  [[nodiscard]] bool showWindow(QQuickWindow &window, QScreen *screen,
                                bool requestActivation,
                                QString *error) override {
    if (screen == nullptr) {
      if (error != nullptr) {
        *error = translated("A display is no longer available.");
      }
      return false;
    }
    window.setScreen(screen);
    window.showFullScreen();
    if (!window.isVisible()) {
      if (error != nullptr) {
        *error =
            translated("The window system refused to show a preview window.");
      }
      return false;
    }
    if (requestActivation) {
      window.requestActivate();
    }
    return true;
  }

  [[nodiscard]] bool isActive(const QQuickWindow &window) const override {
    return window.isActive();
  }

  [[nodiscard]] int activationTimeoutMilliseconds() const noexcept override {
    return kDefaultActivationTimeoutMilliseconds;
  }

  [[nodiscard]] int maximumDurationMilliseconds() const noexcept override {
    return kMaximumPreviewDurationMilliseconds;
  }
};

class BlankPreviewInputSurface final : public QQuickItem {
public:
  BlankPreviewInputSurface(std::function<void()> dismiss, QQuickItem *parent)
      : QQuickItem(parent), m_dismiss(std::move(dismiss)) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    event->accept();
    m_dismiss();
  }

  void hoverMoveEvent(QHoverEvent *event) override {
    event->accept();
    m_dismiss();
  }

private:
  std::function<void()> m_dismiss;
};

class BlankPreviewWindow final : public QQuickWindow {
public:
  explicit BlankPreviewWindow(std::function<void()> dismiss)
      : m_dismiss(std::move(dismiss)) {
    setObjectName(QStringLiteral("qindaqtBlankScreensaverPreview"));
    setTitle(tr("Screen saver preview"));
    setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setColor(Qt::black);

    auto *surface = new BlankPreviewInputSurface(m_dismiss, contentItem());
    surface->setWidth(width());
    surface->setHeight(height());
    connect(this, &QQuickWindow::widthChanged, surface,
            [this, surface] { surface->setWidth(width()); });
    connect(this, &QQuickWindow::heightChanged, surface,
            [this, surface] { surface->setHeight(height()); });
  }

protected:
  void keyPressEvent(QKeyEvent *event) override {
    event->accept();
    m_dismiss();
  }

private:
  std::function<void()> m_dismiss;
};

} // namespace

class ProcessScreensaverPreview::BlankPreview final : public QObject {
public:
  using Completion = std::function<void(QString)>;

  BlankPreview(ScreensaverPreviewEnvironment &environment, Completion completed)
      : m_environment(environment), m_completed(std::move(completed)),
        m_lastPointerPosition(QCursor::pos()) {
    m_focusTimer.setSingleShot(true);
    connect(&m_focusTimer, &QTimer::timeout, this, [this] {
      if (m_activationWindow != nullptr &&
          !m_environment.isActive(*m_activationWindow)) {
        dismiss(translated("The blank preview could not receive keyboard "
                           "focus, so it was closed."));
      }
    });
    if (QCoreApplication::instance() != nullptr) {
      QCoreApplication::instance()->installEventFilter(this);
    }
    if (auto *const application =
            qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
      connect(application, &QGuiApplication::screenAdded, this,
              [this](QScreen *) {
                if (!m_windows.empty()) {
                  dismiss(translated("The display configuration changed, so "
                                     "the preview was closed."));
                }
              });
      connect(application, &QGuiApplication::screenRemoved, this,
              [this](QScreen *) {
                if (!m_windows.empty()) {
                  dismiss(translated("The display configuration changed, so "
                                     "the preview was closed."));
                }
              });
    }
  }

  ~BlankPreview() override {
    m_focusTimer.stop();
    if (QCoreApplication::instance() != nullptr) {
      QCoreApplication::instance()->removeEventFilter(this);
    }
  }

  [[nodiscard]] bool show(QString *error) {
    const QList<QScreen *> availableScreens = m_environment.screens();
    if (availableScreens.isEmpty()) {
      if (error != nullptr) {
        *error = translated(
            "The blank preview could not find an available display.");
      }
      return false;
    }

    for (qsizetype index = 0; index < availableScreens.size(); ++index) {
      QScreen *const screen = availableScreens.at(index);
      auto window = std::make_unique<BlankPreviewWindow>([this] { dismiss(); });
      BlankPreviewWindow *const shownWindow = window.get();
      m_windows.emplace_back(std::move(window));

      QString showError;
      const bool requestedActivation = index == 0;
      if (!m_environment.showWindow(*shownWindow, screen, requestedActivation,
                                    &showError) ||
          !shownWindow->isVisible()) {
        if (error != nullptr) {
          const QString reason = showError.isEmpty()
                                     ? translated("the window was not visible")
                                     : showError;
          *error =
              translated(
                  "The blank preview could not be shown on every display: %1")
                  .arg(reason);
        }
        closeWindows();
        return false;
      }

      if (requestedActivation) {
        m_activationWindow = shownWindow;
        connect(shownWindow, &QWindow::activeChanged, this, [this] {
          if (m_activationWindow != nullptr &&
              m_environment.isActive(*m_activationWindow)) {
            m_focusTimer.stop();
          }
        });
      }
    }

    const int timeout = qMax(1, m_environment.activationTimeoutMilliseconds());
    if (m_activationWindow != nullptr &&
        !m_environment.isActive(*m_activationWindow)) {
      m_focusTimer.start(timeout);
    }
    return true;
  }

  void dismiss(QString failure = {}) {
    if (m_dismissing) {
      return;
    }
    m_dismissing = true;
    m_focusTimer.stop();
    closeWindows();
    m_completed(std::move(failure));
  }

private:
  bool eventFilter(QObject *watched, QEvent *event) override {
    Q_UNUSED(watched)
    if (!m_dismissing && event->type() == QEvent::MouseMove) {
      const auto *mouseEvent = static_cast<QMouseEvent *>(event);
      const QPoint position = mouseEvent->globalPosition().toPoint();
      if (position != m_lastPointerPosition) {
        m_lastPointerPosition = position;
        dismiss();
      }
    }
    return false;
  }

  void closeWindows() {
    for (const auto &window : m_windows) {
      window->close();
    }
  }

  ScreensaverPreviewEnvironment &m_environment;
  Completion m_completed;
  std::vector<std::unique_ptr<BlankPreviewWindow>> m_windows;
  QTimer m_focusTimer;
  QQuickWindow *m_activationWindow = nullptr;
  QPoint m_lastPointerPosition;
  bool m_dismissing = false;
};

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, QObject *parent)
    : ProcessScreensaverPreview(
          catalog, [](const QString &program) { return program; },
          std::make_unique<QtScreensaverPreviewEnvironment>(), parent) {}

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, ProgramResolver programResolver,
    QObject *parent)
    : ProcessScreensaverPreview(
          catalog, std::move(programResolver),
          std::make_unique<QtScreensaverPreviewEnvironment>(), parent) {}

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, ProgramResolver programResolver,
    std::unique_ptr<ScreensaverPreviewEnvironment> environment, QObject *parent)
    : ScreensaverPreview(parent), m_catalog(catalog),
      m_programResolver(std::move(programResolver)),
      m_environment(std::move(environment)),
      m_idleLockInhibitor(
          std::make_unique<ScreensaverPreviewIdleLockInhibitor>()) {
  if (!m_programResolver) {
    m_programResolver = [](const QString &program) { return program; };
  }
  if (!m_environment) {
    m_environment = std::make_unique<QtScreensaverPreviewEnvironment>();
  }
  m_previewDeadlineTimer.setSingleShot(true);
  connect(&m_previewDeadlineTimer, &QTimer::timeout, this, [this] {
    if (m_blankPreview != nullptr) {
      m_blankPreview->dismiss();
      return;
    }
    if (m_process.state() == QProcess::NotRunning) {
      return;
    }
    m_stopRequested = true;
    m_process.terminate();
    m_processKillTimer.start(kProcessStopGraceMilliseconds);
  });
  m_processKillTimer.setSingleShot(true);
  connect(&m_processKillTimer, &QTimer::timeout, this, [this] {
    if (m_process.state() != QProcess::NotRunning) {
      m_process.kill();
    }
  });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
            m_previewDeadlineTimer.stop();
            m_processKillTimer.stop();
            const bool requestedStop = m_stopRequested;
            m_stopRequested = false;
            m_idleLockInhibitor->release();
            if (!requestedStop &&
                (exitStatus != QProcess::NormalExit || exitCode != 0)) {
              const QString result = exitStatus == QProcess::CrashExit
                                         ? translated("crashed")
                                         : translated("exited normally");
              Q_EMIT failed(translated("The screensaver %1 with exit code %2.")
                                .arg(result)
                                .arg(exitCode));
            }
            Q_EMIT finished();
          });
}

ProcessScreensaverPreview::~ProcessScreensaverPreview() {
  m_previewDeadlineTimer.stop();
  m_processKillTimer.stop();
  m_blankPreview.reset();
  if (m_process.state() != QProcess::NotRunning) {
    m_stopRequested = true;
    m_process.terminate();
    if (!m_process.waitForFinished(kProcessStopGraceMilliseconds)) {
      m_process.kill();
      m_process.waitForFinished(kProcessStopGraceMilliseconds);
    }
  }
  m_idleLockInhibitor->release();
}

ScreensaverPreview::Kind
ProcessScreensaverPreview::kindFor(const QString &token) const {
  if (token.isEmpty() || token == ScreensaverPreferences::noneToken()) {
    return Kind::Unavailable;
  }
  if (token == ScreensaverPreferences::blankToken()) {
    return Kind::BlackWindow;
  }
  const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(token);
  return entry.has_value() ? Kind::SaverProgram : Kind::Unavailable;
}

bool ProcessScreensaverPreview::running() const {
  return m_starting || m_process.state() != QProcess::NotRunning ||
         m_blankPreview != nullptr;
}

bool ProcessScreensaverPreview::start(const QString &token, QString *error) {
  if (running()) {
    if (error != nullptr) {
      *error = tr("A preview is already open.");
    }
    return false;
  }
  m_starting = true;
  m_stopRequested = false;

  if (kindFor(token) == Kind::BlackWindow) {
    if (!m_idleLockInhibitor->acquire(error)) {
      m_starting = false;
      return false;
    }
    m_blankPreview =
        std::make_unique<BlankPreview>(*m_environment, [this](QString failure) {
          BlankPreview *const completed = m_blankPreview.get();
          QMetaObject::invokeMethod(
              this,
              [this, completed, failure = std::move(failure)] {
                if (completed == nullptr || m_blankPreview.get() != completed) {
                  return;
                }
                m_blankPreview.reset();
                m_previewDeadlineTimer.stop();
                m_idleLockInhibitor->release();
                if (!failure.isEmpty()) {
                  Q_EMIT failed(failure);
                }
                Q_EMIT finished();
              },
              Qt::QueuedConnection);
        });
    QString showError;
    if (!m_blankPreview->show(&showError)) {
      m_blankPreview.reset();
      m_idleLockInhibitor->release();
      m_starting = false;
      if (error != nullptr) {
        *error = showError.isEmpty()
                     ? tr("The blank screen preview could not be opened.")
                     : showError;
      }
      return false;
    }
    m_previewDeadlineTimer.start(
        boundedPreviewDurationMilliseconds(*m_environment));
    m_starting = false;
    return true;
  }

  const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(token);
  if (kindFor(token) != Kind::SaverProgram || !entry.has_value()) {
    if (error != nullptr) {
      *error = tr("There is nothing to preview for this choice.");
    }
    m_starting = false;
    return false;
  }
  const QString program = m_programResolver(entry->token);
  if (program.isEmpty()) {
    if (error != nullptr) {
      *error = tr("The screensaver program could not be resolved.");
    }
    m_starting = false;
    return false;
  }
  if (!m_idleLockInhibitor->acquire(error)) {
    m_starting = false;
    return false;
  }

  // AGENT-CONTRACT: the session idle launcher starts this same catalog token
  // with this same public argument list. Preview does not inspect lock-scene
  // metadata and cannot route a saver through the lock-screen greeter.
  m_process.start(program, entry->arguments);
  if (!m_process.waitForStarted(3000)) {
    m_idleLockInhibitor->release();
    m_starting = false;
    if (error != nullptr) {
      *error = tr("The screensaver could not start: %1")
                   .arg(m_process.errorString());
    }
    return false;
  }
  m_previewDeadlineTimer.start(
      boundedPreviewDurationMilliseconds(*m_environment));
  m_starting = false;
  return true;
}

} // namespace QindaQt::Apps::SettingsScreensaver
