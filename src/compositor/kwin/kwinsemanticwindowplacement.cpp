// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinsemanticwindowplacement.h"
#include "managedwindowregistry.h"
#include <QScopedValueRollback>
#include <core/output.h>
#include <qindaqt/window_management/command.h>
#include <window.h>
#include <workspace.h>
namespace QindaQt::Compositor::KWinIntegration {
namespace {
bool admit(const KWin::Window *window, QString *error) {
  if (!window || window->isDeleted() || !window->isNormalWindow() ||
      window->isMinimized() || !window->isResizable() || !window->output()) {
    if (error)
      *error =
          QStringLiteral("a live visible resizable normal window is required");
    return false;
  }
  return true;
}
} // namespace
KWinSemanticWindowPlacement::KWinSemanticWindowPlacement(
    ManagedWindowRegistry &registry, QObject *parent)
    : QObject(parent), m_registry(registry) {
  connect(&m_registry, &ManagedWindowRegistry::managedWindowClosed, this,
          [this](const QString &id) {
            m_states.remove(id);
            m_observedIds.remove(id);
          });
  connect(&m_registry, &ManagedWindowRegistry::outputsChanged, this,
          [this] { refresh(); });
  connect(
      KWin::workspace(), &KWin::Workspace::aboutToRearrange, this,
      [this] { refresh(); }, Qt::QueuedConnection);
}
void KWinSemanticWindowPlacement::observe(const QString &id) {
  if (m_observedIds.contains(id))
    return;
  m_observedIds.insert(id);
  auto *window = m_registry.window(id);
  connect(window, &KWin::Window::frameGeometryChanged, this, [this, id] {
    if (m_applying || !m_states.contains(id))
      return;
    const auto *subject = m_registry.window(id);
    if (!subject) {
      m_states.remove(id);
      return;
    }
    // AGENT-GUARD: an SSD/client configure at fractional scale can acknowledge
    // the requested integer frame within one physical pixel. Treat that as
    // the same placement or its acknowledgement would discard restore state.
    const QRectF actual = subject->moveResizeGeometry();
    const QRectF requested = m_states.value(id).target;
    const qreal tolerance =
        1.0 / (subject->output() ? subject->output()->scale() : 1.0) + 1e-6;
    if (qAbs(actual.left() - requested.left()) > tolerance ||
        qAbs(actual.top() - requested.top()) > tolerance ||
        qAbs(actual.right() - requested.right()) > tolerance ||
        qAbs(actual.bottom() - requested.bottom()) > tolerance)
      m_states.remove(id);
  });
  connect(window, &KWin::Window::interactiveMoveResizeStarted, this,
          [this, id] { m_states.remove(id); });
  connect(window, &KWin::Window::maximizedChanged, this, [this, id] {
    if (!m_applying)
      m_states.remove(id);
  });
  connect(window, &KWin::Window::fullScreenChanged, this, [this, id] {
    if (!m_applying)
      m_states.remove(id);
  });
}
bool KWinSemanticWindowPlacement::maximize(const QString &id, double fraction,
                                           QString *error) {
  auto *window = m_registry.window(id);
  if (!admit(window, error))
    return false;
  const auto target = WindowManagement::regionalFrame(
      KWin::workspace()->clientArea(KWin::MaximizeArea, window).toAlignedRect(),
      WindowManagement::insetRegion(fraction));
  if (!target) {
    if (error)
      *error = QStringLiteral("window has no valid maximize area");
    return false;
  }
  const bool existing = m_states.contains(id);
  const QRectF restore =
      existing ? m_states.value(id).restore
               : (window->isFullScreen()
                      ? QRectF(window->fullscreenGeometryRestore())
                  : window->maximizeMode() != KWin::MaximizeRestore
                      ? QRectF(window->geometryRestore())
                      : QRectF(window->frameGeometry()));
  if (!restore.isValid()) {
    if (error)
      *error = QStringLiteral("window has no valid restore frame");
    return false;
  }
  QScopedValueRollback<bool> guard(m_applying, true);
  window->setFullScreen(false);
  window->maximize(KWin::MaximizeRestore);
  m_states.insert(id, {restore, *target, fraction});
  if (!existing)
    observe(id);
  window->moveResize(QRectF(*target));
  return true;
}
bool KWinSemanticWindowPlacement::place(const QString &id, const QRect &frame,
                                        QString *error) {
  auto *window = m_registry.window(id);
  if (!admit(window, error) || !frame.isValid())
    return false;
  QScopedValueRollback<bool> guard(m_applying, true);
  m_states.remove(id);
  window->setFullScreen(false);
  window->maximize(KWin::MaximizeRestore);
  window->moveResize(QRectF(frame));
  return true;
}
bool KWinSemanticWindowPlacement::restore(const QString &id, QString *error) {
  auto *window = m_registry.window(id);
  if (!admit(window, error) || !m_states.contains(id))
    return false;
  const auto state = m_states.take(id);
  QScopedValueRollback<bool> guard(m_applying, true);
  window->moveResize(QRectF(state.restore));
  return true;
}
bool KWinSemanticWindowPlacement::isMaximized(const QString &id) const {
  return m_states.contains(id);
}
void KWinSemanticWindowPlacement::refresh() {
  for (const auto &id : m_states.keys()) {
    if (!m_registry.owner(id).isEmpty()) {
      m_states.remove(id);
      continue;
    }
    QString error;
    static_cast<void>(maximize(id, m_states.value(id).fraction, &error));
  }
}
} // namespace QindaQt::Compositor::KWinIntegration
