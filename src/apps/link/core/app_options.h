// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace QindaQt::LinkApp {

enum class Mode { Main, Reply };

// Command line of `qindaqt-link`:
//   qindaqt-link [--page <name>] [--size WxH] [--fake <scenario>] [--grab <png> [--grab-after <ms>]]
//   qindaqt-link --reply -- <device> <key> <action>
// The notification path launches the reply form with exactly the second shape; the `--` keeps a
// device id or key that starts with a dash from being read as an option.
struct AppOptions {
    Mode mode = Mode::Main;
    QString page = QStringLiteral("devices");
    QSize size;                       // invalid = the window's default
    QString fakeScenario;             // empty = the real session bus
    QString grabPath;
    int grabAfterMs = 1200;
    qint64 openThread = -1;           // --open-thread N (with --fake): open that conversation, for renders
    QString device, key, action;      // Reply mode
    [[nodiscard]] bool fake() const { return !fakeScenario.isEmpty(); }
    // Offscreen runs must never map a window on the user's desktop.
    [[nodiscard]] bool headless() const { return !grabPath.isEmpty(); }
};

[[nodiscard]] QStringList pageIds();
// Parses arguments including argv[0]. Returns nullopt with *error set on bad input.
[[nodiscard]] std::optional<AppOptions> parseAppOptions(const QStringList &arguments, QString *error);

} // namespace QindaQt::LinkApp
