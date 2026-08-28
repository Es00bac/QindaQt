// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell {
class ApplicationCoordinator;
struct PortalRequest;
} // namespace QindaQt::AppShell

namespace QindaQt::Apps::TextEditor {

// AGENT-CONTRACT: An implementation must call
// ApplicationCoordinator::resolvePortal for the exact given request ID
// before returning, or the coordinator's one pending portal request never
// completes and every later Open/Save As silently fails Busy. An
// implementation that cannot obtain a real selection must resolve with
// accepted = false; it must never invent a path.
class FileSelectionAdapter {
public:
  virtual ~FileSelectionAdapter() = default;

  virtual void
  presentOpenFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) = 0;
  virtual void
  presentSaveFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) = 0;
};

} // namespace QindaQt::Apps::TextEditor
