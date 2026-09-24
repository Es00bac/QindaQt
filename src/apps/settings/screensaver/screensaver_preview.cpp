// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>

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

#include <optional>
#include <utility>
#include <vector>

using QindaQt::Session::DesktopControls::ScreensaverCatalog;
using QindaQt::Session::DesktopControls::ScreensaverCatalogEntry;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

namespace QindaQt::Apps::SettingsScreensaver {
namespace {

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
  explicit BlankPreview(std::function<void()> dismissed)
      : m_dismissed(std::move(dismissed)), m_lastPointerPosition(QCursor::pos()) {
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
      m_windows.emplace_back(
          std::make_unique<BlankPreviewWindow>([this] { dismiss(); }));
    } else {
      for (QScreen *screen : screens) {
        auto window =
            std::make_unique<BlankPreviewWindow>([this] { dismiss(); });
        window->setScreen(screen);
        m_windows.emplace_back(std::move(window));
      }
    }
    if (QCoreApplication::instance() != nullptr) {
      QCoreApplication::instance()->installEventFilter(this);
    }
  }

  ~BlankPreview() override {
    if (QCoreApplication::instance() != nullptr) {
      QCoreApplication::instance()->removeEventFilter(this);
    }
  }

  [[nodiscard]] bool show() {
    for (const auto &window : m_windows) {
      window->showFullScreen();
    }
    if (!m_windows.empty()) {
      m_windows.front()->requestActivate();
    }
    for (const auto &window : m_windows) {
      if (window->isVisible())
        return true;
    }
    return false;
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

  void dismiss() {
    if (m_dismissing)
      return;
    m_dismissing = true;
    for (const auto &window : m_windows) {
      window->close();
    }
    m_dismissed();
  }

  std::function<void()> m_dismissed;
  std::vector<std::unique_ptr<BlankPreviewWindow>> m_windows;
  QPoint m_lastPointerPosition;
  bool m_dismissing = false;
};

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, QObject *parent)
    : ProcessScreensaverPreview(
          catalog, [](const QString &program) { return program; }, parent) {}

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, ProgramResolver programResolver,
    QObject *parent)
    : ScreensaverPreview(parent), m_catalog(catalog),
      m_programResolver(std::move(programResolver)) {
  if (!m_programResolver) {
    m_programResolver = [](const QString &program) { return program; };
  }
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this] { Q_EMIT finished(); });
}

ProcessScreensaverPreview::~ProcessScreensaverPreview() {
  m_blankPreview.reset();
  if (m_process.state() != QProcess::NotRunning) {
    m_process.terminate();
    if (!m_process.waitForFinished(500)) {
      m_process.kill();
      m_process.waitForFinished(500);
    }
  }
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
  return m_process.state() != QProcess::NotRunning || m_blankPreview != nullptr;
}

bool ProcessScreensaverPreview::start(const QString &token, QString *error) {
  if (running()) {
    if (error != nullptr) {
      *error = tr("A preview is already open.");
    }
    return false;
  }
  const Kind kind = kindFor(token);
  if (kind == Kind::BlackWindow) {
    m_blankPreview = std::make_unique<BlankPreview>([this] {
      BlankPreview *const completed = m_blankPreview.get();
      QMetaObject::invokeMethod(
          this,
          [this, completed] {
            if (completed == nullptr || m_blankPreview.get() != completed) {
              return;
            }
            m_blankPreview.reset();
            Q_EMIT finished();
          },
          Qt::QueuedConnection);
    });
    if (m_blankPreview->show())
      return true;
    m_blankPreview.reset();
    if (error != nullptr) {
      *error = tr("The blank screen preview could not be opened.");
    }
    return false;
  }

  const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(token);
  if (kind != Kind::SaverProgram || !entry.has_value()) {
    if (error != nullptr) {
      *error = tr("There is nothing to preview for this choice.");
    }
    return false;
  }
  const QString program = m_programResolver(entry->token);
  if (program.isEmpty()) {
    if (error != nullptr) {
      *error = tr("The screensaver program could not be resolved.");
    }
    return false;
  }

  // AGENT-CONTRACT: the session idle launcher starts this same catalog token
  // with this same public argument list. Preview does not inspect lock-scene
  // metadata and cannot route a saver through the lock-screen greeter.
  m_process.start(program, entry->arguments);
  if (!m_process.waitForStarted(3000)) {
    if (error != nullptr) {
      *error = tr("The screensaver could not start: %1")
                   .arg(m_process.errorString());
    }
    return false;
  }
  return true;
}

} // namespace QindaQt::Apps::SettingsScreensaver
