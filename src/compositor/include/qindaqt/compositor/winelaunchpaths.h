// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace QindaQt::Compositor {

// AGENT-CONTRACT: pure Windows-to-host path mapping for the PE-icon half of
// ADR-0230. The compositor knows a Wine window's command line (read through
// KWin's authenticated PID, ADR-0169); reaching the executable's PE resources
// means mapping the WINDOWS path it names onto the host filesystem. Every
// function here is a pure string transform with explicit refusal cases - no
// filesystem is touched, so hostile paths fail closed in tests.

// The FULL argument naming the last `.exe` on a Wine/Proton command line
// (e.g. `C:\Games\Foo\Foo.exe`, `Z:\home\user\foo.exe`, or a POSIX path), as
// opposed to executableFromCommandLine's basename. Same acceptance rules for
// the argument itself: bounded, no control characters. Empty when the command
// line names no Windows executable at all.
[[nodiscard]] QString windowsExecutablePathFromCommandLine(const QByteArray &cmdline);

// The Wine prefix for a client from its `/proc/<pid>/environ` bytes
// (NUL-separated KEY=VALUE). A valid WINEPREFIX (absolute, bounded, no
// control characters) wins; when the variable is absent the Wine default
// `<homeDir>/.wine` answers; a PRESENT but unusable value fails closed to an
// empty string - guessing a prefix from a corrupt environment would read the
// wrong filesystem tree.
[[nodiscard]] QString winePrefixFromEnviron(const QByteArray &environ,
                                            const QString &homeDir);

// Host path for a Windows executable path inside `winePrefix`:
// - `C:` (any case, either separator) maps beneath `<prefix>/drive_c`; a
//   cleaned result escaping that directory is refused.
// - `Z:` maps to the host root (Wine's universal drive mapping).
// - A path already absolute on the host (`/...`) passes through unchanged
//   (Proton launchers sometimes hand Wine host paths).
// - Any other drive letter, a UNC path, or a bare name yields empty.
// Symlink resolution and containment re-checks stay with the caller; this is
// the pure lexical contract those checks are applied over.
[[nodiscard]] QString hostPathForWindowsExecutable(const QString &windowsPath,
                                                   const QString &winePrefix);

} // namespace QindaQt::Compositor
