// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_about_computer/about_computer_info.h>

namespace QindaQt::Apps::SettingsAboutComputer {

// Injectable info source so the model is testable without a system bus,
// /proc, or a Portage database -- mirrors the Store/Model split used by the
// Default applications route.
class AboutComputerInfoSource {
public:
  virtual ~AboutComputerInfoSource() = default;
  [[nodiscard]] virtual AboutComputerInfo read() const = 0;
};

// AGENT-CONTRACT: the sole D-Bus/filesystem boundary for this route (the
// allow-list boundary scan restricts the D-Bus modules to this file). Every read
// is synchronous and best-effort: a source that fails or is unavailable
// (no system bus, non-Gentoo host, an unreadable package database) reports
// its own "unavailable" state rather than failing the whole read. No
// mutation, no subscription, no daemon started or stopped.
class SystemAboutComputerInfoSource final : public AboutComputerInfoSource {
public:
  [[nodiscard]] AboutComputerInfo read() const override;
};

} // namespace QindaQt::Apps::SettingsAboutComputer
