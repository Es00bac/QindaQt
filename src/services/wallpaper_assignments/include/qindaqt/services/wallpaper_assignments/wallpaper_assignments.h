// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Services::WallpaperAssignments {

// Settings1 key (schema v2, `appearance` domain, object, default `{}`). It
// holds every per-display and per-desktop wallpaper choice; the wallpaper
// shown everywhere else stays `appearance.wallpaper` (ADR-0286).
inline constexpr auto SettingsKey = "appearance.wallpaperAssignments";

// AGENT-CONTRACT: the hostile-input bounds of the stored value. The shell
// reads and Appearance Settings writes it only through this codec; a second
// copy of any limit would be a second authority that can drift.
namespace Bounds {
inline constexpr qint64 formatVersion = 1;
inline constexpr qsizetype maxAssignments = 64;
// ADR-0017 stable output ids (DisplayIdentity::kMaxStableIdUtf8Bytes).
inline constexpr qsizetype maxDisplayIdUtf8Bytes = 128;
// Compositor desktop ids, in UTF-16 units like
// Shell::Workspaces::Bounds::maxIdLength.
inline constexpr qsizetype maxDesktopIdLength = 128;
// AGENT-GUARD: sized so the largest admissible value (every record at every
// bound) stays inside Settings1's aggregate value bound
// (WireContract::MaximumAggregateValueBytes); the codec test asserts it. A
// larger limit would admit choices the service then refuses to store.
inline constexpr qsizetype maxWallpaperUtf8Bytes = 2048;
} // namespace Bounds

// One saved choice. `display` is an ADR-0017 stable output id, or empty for
// every display; `desktop` is a compositor virtual-desktop id, or empty for
// every desktop. Both empty would be the everywhere wallpaper, which lives in
// `appearance.wallpaper` and is never stored here. `wallpaper` uses the forms
// of `appearance.wallpaper`: `qindaqt:<name>` or an absolute path; empty means
// no wallpaper (the fallback color) for that scope.
struct WallpaperAssignment final {
  QString display;
  QString desktop;
  QString wallpaper;

  [[nodiscard]] bool operator==(const WallpaperAssignment &) const = default;
};

// Which saved choice a resolution used, most specific first.
enum class ResolvedScope {
  DisplayDesktop,
  Display,
  Desktop,
  Everywhere,
};

struct WallpaperResolution final {
  QString wallpaper;
  ResolvedScope scope = ResolvedScope::Everywhere;

  [[nodiscard]] bool operator==(const WallpaperResolution &) const = default;
};

enum class WallpaperEditError {
  None,
  // A malformed display or desktop id, or the everywhere scope.
  InvalidScope,
  // Not empty, `qindaqt:<name>`, or a bounded absolute path.
  InvalidWallpaper,
  // A new scope would exceed Bounds::maxAssignments.
  Full,
};

struct WallpaperAssignmentsDecodeResult;

// The saved choices. A pure value: no Settings access, filesystem, or
// identity lookup. Every edit validates first and leaves the value unchanged
// when it fails, so callers edit a copy and publish it only on success.
//
// AGENT-GUARD: records stay sorted by (display, desktop) and each scope
// appears once, so equal choice sets compare and encode byte-identically.
// Settings' draft diff and the shell's change detection depend on that.
class WallpaperAssignments final {
public:
  [[nodiscard]] const QList<WallpaperAssignment> &assignments() const noexcept
  {
    return m_assignments;
  }
  [[nodiscard]] bool isEmpty() const noexcept { return m_assignments.isEmpty(); }
  [[nodiscard]] qsizetype size() const noexcept { return m_assignments.size(); }

  // The choice saved for exactly this scope, if any.
  [[nodiscard]] std::optional<QString> find(const QString &display,
                                            const QString &desktop) const;
  // Adds or replaces the choice for one scope.
  WallpaperEditError set(const QString &display, const QString &desktop,
                         const QString &wallpaper);
  // Removes the choice for one scope; false when none was saved.
  bool remove(const QString &display, const QString &desktop);

  // AGENT-CONTRACT (ADR-0286 precedence): the wallpaper one output paints.
  // `display` is the output's stable id and `desktop` the current desktop id;
  // either may be empty when unknown (Display1 or the compositor has not
  // answered, the identity is ambiguous, or the desktop is gone). Order: the
  // display-and-desktop choice, the display's choice, the desktop's choice on
  // every display, then `everywhere`. Unknown or removed displays and desktops
  // therefore fall back without deleting any saved choice. The shell and the
  // Appearance preview both call this; never re-implement the order.
  [[nodiscard]] WallpaperResolution resolve(const QString &everywhere,
                                            const QString &display,
                                            const QString &desktop) const;

  [[nodiscard]] bool operator==(const WallpaperAssignments &) const = default;

  // Strict codec for the Settings1 value (ADR-0286 has the shape). An absent
  // value and the schema default `{}` decode as no choices. Any other shape,
  // unknown field, duplicate scope, or bound violation rejects the whole
  // value; partial choices never enter a model.
  [[nodiscard]] static WallpaperAssignmentsDecodeResult
  decodeSettingsValue(const QVariant &value);
  // `{}` for no choices (the schema default), else
  // {"version": 1, "assignments": [{"display", "desktop", "wallpaper"}...]}.
  [[nodiscard]] static QVariantMap encodeSettingsValue(const WallpaperAssignments &value);
  // Validates a complete record list against every rule above.
  [[nodiscard]] static std::optional<WallpaperAssignments>
  fromAssignments(const QList<WallpaperAssignment> &assignments);

  [[nodiscard]] static bool isValidDisplayId(const QString &display);
  [[nodiscard]] static bool isValidDesktopId(const QString &desktop);
  [[nodiscard]] static bool isValidWallpaper(const QString &wallpaper);

private:
  QList<WallpaperAssignment> m_assignments;
};

struct WallpaperAssignmentsDecodeResult final {
  std::optional<WallpaperAssignments> value;
  QString error;

  [[nodiscard]] bool ok() const noexcept { return value.has_value(); }
};

} // namespace QindaQt::Services::WallpaperAssignments
