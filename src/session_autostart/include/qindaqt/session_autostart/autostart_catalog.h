// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QProcessEnvironment>

namespace QindaQt::SessionAutostart {

// The same explicit roots and execution environment are supplied to Settings
// and the session. A private test passes only disposable directories; scanning
// never falls back to ambient XDG paths.
struct ScanOptions final {
    QString userDirectory;
    QStringList systemDirectories;
    QStringList desktops;
    QStringList executableDirectories;
    QString terminalExecutable = QStringLiteral("qqterm");

    [[nodiscard]] static ScanOptions fromEnvironment(
        const QProcessEnvironment &environment = QProcessEnvironment::systemEnvironment());
};

struct Entry final {
    QString id;                 // basename without .desktop
    QString sourcePath;         // winner after user/system basename shadowing
    QString name;
    QString comment;
    QString iconName;
    QString exec;
    QString ineligibilityReason;
    QString program;            // resolved absolute executable, when eligible
    QStringList arguments;      // bounded desktop-entry expansion, never a shell
    QString workingDirectory;
    bool enabled = false;       // only user-facing disable switches
    bool eligible = false;      // all launch conditions, including enabled
    bool custom = false;

    friend bool operator==(const Entry &, const Entry &) = default;
};

// Read-only XDG autostart merge, eligibility and launch plan. No process is
// started here. The first basename masks lower-priority entries even when its
// content is malformed or Hidden=true. Valid Application entries with Name
// are projected so Settings can explain eligibility; invalid file types are
// never presented or launched.
//
// AGENT-CONTRACT: SessionProcessSupervisor consumes only eligible plans from
// this scan. XdgAutostartStore projects these exact enabled/eligible decisions;
// do not add a second parser or a Settings-specific launch rule.
[[nodiscard]] QList<Entry> scan(const ScanOptions &options,
                                QString *error = nullptr);

} // namespace QindaQt::SessionAutostart
