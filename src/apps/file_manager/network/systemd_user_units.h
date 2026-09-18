// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: the one seam through which File Manager asks the user's own
// service manager to notice a unit it wrote (ADR-0199). It is deliberately
// tiny and one-directional: reload, enable, disable. There is no query, no
// status, no start and no stop, because the file manager does not own when a
// network mount is active -- login does, and the user does through the
// ordinary `systemctl --user` commands.
//
// AGENT-GUARD: every implementation acts on the *user* manager and on units
// under the user's own configuration directory. Nothing here may reach the
// system manager, run as another user, or accept a unit name it did not get
// from MountUnit::build().
class SystemdUserUnits {
public:
  virtual ~SystemdUserUnits() = default;

  // Each returns an empty string on success, or a bounded human-readable
  // reason. An implementation must never block the GUI thread for longer than
  // its own short timeout, and must never prompt.
  [[nodiscard]] virtual QString reload() = 0;
  [[nodiscard]] virtual QString enable(const QString &unitName) = 0;
  [[nodiscard]] virtual QString disable(const QString &unitName) = 0;
};

using SystemdUserUnitsPtr = std::unique_ptr<SystemdUserUnits>;

// Production implementation: runs `systemctl --user` with a short timeout.
// AGENT-NOTE: it is composed only when the mount knob is actually used, and
// no test ever constructs it -- a test that did would touch the developer's
// own live user manager.
class SystemctlUserUnits final : public SystemdUserUnits {
public:
  // Beyond this a call is abandoned and reported, so a wedged service manager
  // cannot freeze the window.
  static constexpr int timeoutMilliseconds = 5000;

  [[nodiscard]] QString reload() override;
  [[nodiscard]] QString enable(const QString &unitName) override;
  [[nodiscard]] QString disable(const QString &unitName) override;

private:
  [[nodiscard]] static QString run(const QStringList &arguments);
};

} // namespace QindaQt::Apps::FileManager
