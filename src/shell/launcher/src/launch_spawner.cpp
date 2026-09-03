// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launch_spawner.h"

#include <QProcess>

namespace QindaQt::Shell::Launcher {
namespace {

bool isForwardedVariable(const QString &name)
{
  // AGENT-CONTRACT: This allowlist is the launcher's child-environment
  // authority (documented in docs/wiki/shell/launcher.md). Session identity,
  // locale, display, and runtime discovery survive; everything else —
  // including QindaQt's own development overrides and any caller-injected
  // variables — is stripped before the child starts.
  static const QStringList exact {
    QStringLiteral("HOME"),
    QStringLiteral("PATH"),
    QStringLiteral("LANG"),
    QStringLiteral("LC_ALL"),
    QStringLiteral("XDG_RUNTIME_DIR"),
    QStringLiteral("XDG_DATA_DIRS"),
    QStringLiteral("XDG_CONFIG_DIRS"),
    QStringLiteral("XDG_SESSION_TYPE"),
    QStringLiteral("XDG_SESSION_ID"),
    QStringLiteral("XDG_CURRENT_DESKTOP"),
    QStringLiteral("WAYLAND_DISPLAY"),
    QStringLiteral("DISPLAY"),
    QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
    QStringLiteral("QT_QPA_PLATFORM"),
    QStringLiteral("QT_SCALE_FACTOR"),
  };
  return exact.contains(name) || name.startsWith(QLatin1String("LC_"));
}

} // namespace

QProcessEnvironment sanitizedChildEnvironment(const QProcessEnvironment &base)
{
  QProcessEnvironment sanitized;
  for (const QString &name : base.keys()) {
    if (isForwardedVariable(name))
      sanitized.insert(name, base.value(name));
  }
  return sanitized;
}

SpawnResult QProcessLaunchSpawner::spawn(const SpawnRequest &request)
{
  if (request.program.isEmpty()) {
    return { false, QStringLiteral("spawn request has no program") };
  }

  QProcess process;
  process.setProgram(request.program);
  process.setArguments(request.arguments);
  process.setProcessEnvironment(
      sanitizedChildEnvironment(QProcessEnvironment::systemEnvironment()));
  if (!request.workingDirectory.isEmpty())
    process.setWorkingDirectory(request.workingDirectory);

  // AGENT-GUARD: Only the instance startDetached overload honors the
  // per-process environment; the static overloads silently inherit the
  // unsanitized shell environment.
  qint64 pid = 0;
  if (!process.startDetached(&pid)) {
    return { false, QStringLiteral("could not start %1").arg(request.program) };
  }
  return { true, {} };
}

} // namespace QindaQt::Shell::Launcher
