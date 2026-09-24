// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_menu/desktop_surface_commands.h"

namespace QindaQt::Shell::DesktopMenu {
namespace {

// Bounds hostile or runaway QML input: a desktop has one surface per output.
constexpr qsizetype kMaxSurfaces = 32;

} // namespace

DesktopSurfaceCommands::DesktopSurfaceCommands(QObject *parent)
    : QObject(parent)
{
}

QString DesktopSurfaceCommands::primaryScreen() const
{
  for (auto it = m_surfaces.cbegin(); it != m_surfaces.cend(); ++it) {
    if (it.value()) {
      return it.key();
    }
  }
  return {};
}

bool DesktopSurfaceCommands::surfaceAttached() const noexcept
{
  for (const bool primary : m_surfaces) {
    if (primary) {
      return true;
    }
  }
  return false;
}

bool DesktopSurfaceCommands::pasteAvailable() const noexcept
{
  return m_pasteReported && !m_pasteReporter.isEmpty()
      && m_surfaces.value(m_pasteReporter, false);
}

bool DesktopSurfaceCommands::request(Command command)
{
  if (!surfaceAttached()) {
    return false;
  }
  switch (command) {
  case Command::NewFolder:
    Q_EMIT commandRequested(QStringLiteral("new-folder"), true);
    return true;
  case Command::Paste:
    Q_EMIT commandRequested(QStringLiteral("paste"), true);
    return true;
  case Command::SelectAll:
    Q_EMIT commandRequested(QStringLiteral("select-all"), false);
    return true;
  case Command::CleanUp:
    Q_EMIT commandRequested(QStringLiteral("clean-up"), true);
    return true;
  }
  return false;
}

void DesktopSurfaceCommands::attachSurface(const QString &screenName, bool primary)
{
  if (screenName.isEmpty()
      || (!m_surfaces.contains(screenName) && m_surfaces.size() >= kMaxSurfaces)) {
    return;
  }
  const bool attachedBefore = surfaceAttached();
  const bool pasteBefore = pasteAvailable();
  m_surfaces.insert(screenName, primary);
  if (!primary && m_pasteReporter == screenName) {
    m_pasteReporter.clear();
    m_pasteReported = false;
  }
  if (attachedBefore != surfaceAttached() || pasteBefore != pasteAvailable()) {
    Q_EMIT stateChanged();
  }
}

void DesktopSurfaceCommands::detachSurface(const QString &screenName)
{
  const bool attachedBefore = surfaceAttached();
  const bool pasteBefore = pasteAvailable();
  if (m_surfaces.remove(screenName) == 0) {
    return;
  }
  if (m_pasteReporter == screenName) {
    m_pasteReporter.clear();
    m_pasteReported = false;
  }
  if (attachedBefore != surfaceAttached() || pasteBefore != pasteAvailable()) {
    Q_EMIT stateChanged();
  }
}

void DesktopSurfaceCommands::reportPasteAvailable(const QString &screenName, bool available)
{
  // Only the primary surface's own contents controller speaks for Paste: it
  // is the one surface a primary-only paste would run on.
  if (!m_surfaces.value(screenName, false)) {
    return;
  }
  const bool before = pasteAvailable();
  m_pasteReporter = screenName;
  m_pasteReported = available;
  if (before != pasteAvailable()) {
    Q_EMIT stateChanged();
  }
}

} // namespace QindaQt::Shell::DesktopMenu
