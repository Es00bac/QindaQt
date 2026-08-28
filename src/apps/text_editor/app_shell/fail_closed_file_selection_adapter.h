// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_selection_adapter.h"

namespace QindaQt::Apps::TextEditor {

// The safe default: every Open/Save As request is denied without showing
// any surface. This is the adapter EditorAppShellBridge falls back to when
// no real adapter is injected, and the adapter registered/tests-and-offline
// verification use so no host file chooser is ever contacted.
class FailClosedFileSelectionAdapter final : public FileSelectionAdapter {
public:
  void
  presentOpenFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) override;
  void
  presentSaveFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) override;
};

} // namespace QindaQt::Apps::TextEditor
