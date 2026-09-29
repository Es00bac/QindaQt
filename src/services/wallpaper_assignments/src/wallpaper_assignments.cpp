// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"

#include <QChar>

#include <algorithm>

namespace QindaQt::Services::WallpaperAssignments {
namespace {

// Control and format characters (NUL, line breaks, bidi overrides) are never
// part of an identity or path the desktop should act on.
bool hasOnlyVisibleCharacters(const QString &text)
{
  return std::none_of(text.cbegin(), text.cend(), [](QChar character) {
    const QChar::Category category = character.category();
    return category == QChar::Other_Control || category == QChar::Other_Format;
  });
}

bool scopeLess(const WallpaperAssignment &record, const QString &display,
               const QString &desktop)
{
  const int byDisplay = QString::compare(record.display, display);
  return byDisplay < 0 || (byDisplay == 0 && QString::compare(record.desktop, desktop) < 0);
}

bool validScope(const QString &display, const QString &desktop)
{
  if (display.isEmpty() && desktop.isEmpty())
    return false;
  return (display.isEmpty() || WallpaperAssignments::isValidDisplayId(display))
      && (desktop.isEmpty() || WallpaperAssignments::isValidDesktopId(desktop));
}

} // namespace

bool WallpaperAssignments::isValidDisplayId(const QString &display)
{
  // ADR-0017 ids are a source prefix plus hex digits or a safe connector
  // name; whitespace never occurs, so it is rejected with the other
  // invisible characters rather than trimmed into a different identity.
  return !display.isEmpty()
      && display.toUtf8().size() <= Bounds::maxDisplayIdUtf8Bytes
      && hasOnlyVisibleCharacters(display)
      && std::none_of(display.cbegin(), display.cend(),
                      [](QChar character) { return character.isSpace(); });
}

bool WallpaperAssignments::isValidDesktopId(const QString &desktop)
{
  return !desktop.isEmpty() && desktop.size() <= Bounds::maxDesktopIdLength
      && hasOnlyVisibleCharacters(desktop);
}

bool WallpaperAssignments::isValidWallpaper(const QString &wallpaper)
{
  if (wallpaper.isEmpty())
    return true;
  if (wallpaper.toUtf8().size() > Bounds::maxWallpaperUtf8Bytes
      || !hasOnlyVisibleCharacters(wallpaper)) {
    return false;
  }
  // AGENT-CONTRACT: the two forms resolveWallpaperSource() in the shell and
  // the Appearance preview accept (ADR-0078, ADR-0228): a bundled identity
  // with a non-empty name and no '/', or an absolute path.
  static const QString bundledPrefix = QStringLiteral("qindaqt:");
  if (wallpaper.startsWith(bundledPrefix)) {
    const QString name = wallpaper.sliced(bundledPrefix.size());
    return !name.isEmpty() && !name.contains(QLatin1Char('/'));
  }
  return wallpaper.startsWith(QLatin1Char('/'));
}

std::optional<QString> WallpaperAssignments::find(const QString &display,
                                                  const QString &desktop) const
{
  const auto match = std::find_if(
      m_assignments.cbegin(), m_assignments.cend(),
      [&](const WallpaperAssignment &record) {
        return record.display == display && record.desktop == desktop;
      });
  if (match == m_assignments.cend())
    return std::nullopt;
  return match->wallpaper;
}

WallpaperEditError WallpaperAssignments::set(const QString &display,
                                             const QString &desktop,
                                             const QString &wallpaper)
{
  if (!validScope(display, desktop))
    return WallpaperEditError::InvalidScope;
  if (!isValidWallpaper(wallpaper))
    return WallpaperEditError::InvalidWallpaper;
  const auto position = std::find_if(
      m_assignments.begin(), m_assignments.end(),
      [&](const WallpaperAssignment &record) {
        return !scopeLess(record, display, desktop);
      });
  if (position != m_assignments.end() && position->display == display
      && position->desktop == desktop) {
    position->wallpaper = wallpaper;
    return WallpaperEditError::None;
  }
  if (m_assignments.size() >= Bounds::maxAssignments)
    return WallpaperEditError::Full;
  m_assignments.insert(position, WallpaperAssignment{display, desktop, wallpaper});
  return WallpaperEditError::None;
}

bool WallpaperAssignments::remove(const QString &display, const QString &desktop)
{
  return m_assignments.removeIf([&](const WallpaperAssignment &record) {
    return record.display == display && record.desktop == desktop;
  }) > 0;
}

WallpaperResolution WallpaperAssignments::resolve(const QString &everywhere,
                                                  const QString &display,
                                                  const QString &desktop) const
{
  if (!display.isEmpty() && !desktop.isEmpty()) {
    if (const auto choice = find(display, desktop))
      return {*choice, ResolvedScope::DisplayDesktop};
  }
  if (!display.isEmpty()) {
    if (const auto choice = find(display, QString()))
      return {*choice, ResolvedScope::Display};
  }
  if (!desktop.isEmpty()) {
    if (const auto choice = find(QString(), desktop))
      return {*choice, ResolvedScope::Desktop};
  }
  return {everywhere, ResolvedScope::Everywhere};
}

std::optional<WallpaperAssignments>
WallpaperAssignments::fromAssignments(const QList<WallpaperAssignment> &assignments)
{
  if (assignments.size() > Bounds::maxAssignments)
    return std::nullopt;
  WallpaperAssignments result;
  for (const WallpaperAssignment &record : assignments) {
    // AGENT-GUARD: a repeated scope is hostile, not a later-wins edit. Two
    // records for one scope would make the stored meaning order-dependent.
    if (result.find(record.display, record.desktop).has_value())
      return std::nullopt;
    if (result.set(record.display, record.desktop, record.wallpaper)
        != WallpaperEditError::None) {
      return std::nullopt;
    }
  }
  return result;
}

} // namespace QindaQt::Services::WallpaperAssignments
