// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QString>

namespace QindaQt::Services::ScreenshotPreferences {

// The screenshot file-name pattern, without extension.
//
// AGENT-CONTRACT: `{date}` is yyyy-MM-dd, `{time}` is HH-mm-ss and `{mode}`
// is the capture mode id (region, all-screens, ...). Any other text is kept
// literally. The default expands to Screenshot_YYYY-MM-DD_HH-MM-SS, the name
// ADR-0289 promises; Settings validates with the same function the tool
// expands with, so the two can never disagree about a pattern.
inline constexpr char kDefaultFileNamePattern[] = "Screenshot_{date}_{time}";
inline constexpr qsizetype kMaxFileNamePatternLength = 128;

// True when the pattern is non-blank, bounded, and cannot leave its folder
// (no '/', no NUL, not "." or "..") once expanded.
[[nodiscard]] bool isValidFileNamePattern(const QString &pattern);

// The file name (with ".png") for one capture; an invalid pattern falls
// back to the default rather than producing an unsafe name.
[[nodiscard]] QString expandFileNamePattern(const QString &pattern, const QDateTime &time,
                                            const QString &modeId);

// XDG_PICTURES_DIR/Screenshots (HOME/Pictures/Screenshots without one).
[[nodiscard]] QString defaultScreenshotFolder();
// The folder a save uses: the configured one ("~/" expanded), or the
// default when none is configured or it is not an absolute path.
[[nodiscard]] QString resolveScreenshotFolder(const QString &configured);

} // namespace QindaQt::Services::ScreenshotPreferences
