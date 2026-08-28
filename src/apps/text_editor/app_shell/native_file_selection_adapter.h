// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_selection_adapter.h"

class QWidget;

namespace QindaQt::Apps::TextEditor {

// Production adapter: presents the real modal QFileDialog, parented to the
// owning window, and resolves the pending portal request with its outcome.
// This is the only collaborator in the editor that instantiates QFileDialog;
// EditorWindow no longer talks to the platform file chooser directly.
class NativeFileSelectionAdapter final : public FileSelectionAdapter {
public:
  explicit NativeFileSelectionAdapter(QWidget *parent);

  void
  presentOpenFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) override;
  void
  presentSaveFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) override;

private:
  QWidget *m_parent;
};

} // namespace QindaQt::Apps::TextEditor
