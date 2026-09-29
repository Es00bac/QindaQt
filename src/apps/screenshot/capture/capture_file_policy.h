// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QString>

#include <functional>

namespace QindaQt::Screenshot {

// Where and under which name a capture is written.
//
// AGENT-CONTRACT: the folder and name come from the screenshot preferences
// (services.screenshotFolder / services.screenshotFileNamePattern, resolved
// by the screenshot_preferences library; default
// XDG_PICTURES_DIR/Screenshots/Screenshot_YYYY-MM-DD_HH-MM-SS.png, ADR-0289).
// A save never replaces an existing file: a taken name gains "-2", "-3", ...
// before the extension, and the final create is exclusive so a racing
// writer loses the name instead of its contents.

using PathExists = std::function<bool(const QString &path)>;

// The first free name for `fileName` inside `directory`, or empty when the
// collision bound is exhausted.
[[nodiscard]] QString uniquePath(const QString &directory, const QString &fileName,
                                 const PathExists &exists);

struct SaveResult {
    QString path;
    QString error;
    [[nodiscard]] bool ok() const { return error.isEmpty() && !path.isEmpty(); }
};

// Creates `directory` when needed and writes `image` under the first free
// name derived from `fileName`. The image format follows the extension.
[[nodiscard]] SaveResult saveWithoutOverwriting(const QImage &image, const QString &directory,
                                                const QString &fileName);

// Writes to an exact path the user confirmed in a Save As dialog, which has
// already asked about replacing an existing file.
[[nodiscard]] SaveResult saveToConfirmedPath(const QImage &image, const QString &path);

} // namespace QindaQt::Screenshot
