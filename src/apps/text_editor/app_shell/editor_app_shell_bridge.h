// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_selection_adapter.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <memory>
#include <optional>

class QAction;

namespace QindaQt::Apps::TextEditor {

// AGENT-CONTRACT: This is the editor's one collaborator that owns an
// ApplicationCoordinator and its injected file-selection adapter. It never
// decides close consent or command execution itself; EditorWindow keeps
// that authority and only asks this bridge to publish projection and
// mediate portal requests. A null adapter means "fail closed": every
// Open/Save As request is denied without touching any host surface.
class EditorAppShellBridge final : public QObject {
  Q_OBJECT

public:
  explicit EditorAppShellBridge(
      std::unique_ptr<FileSelectionAdapter> fileAdapter,
      QObject *parent = nullptr);

  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &coordinator();

  // Publishes the documented File/Edit action catalog as one atomic
  // replacement. Call exactly once, after the local QAction tree exists.
  [[nodiscard]] QindaQt::AppShell::Error publishActionCatalog();
  [[nodiscard]] QindaQt::AppShell::Error
  setActionEnabled(const QString &actionId, bool enabled);

  // Routes a known AppShell action ID's activationRequested to the local
  // QAction that owns the real command, so an external activation (a
  // future global-menu consumer) runs the identical local trigger path as
  // clicking the menu item.
  void bindActivationTargets(const QHash<QString, QAction *> &targets);

  // Issues an AppShell Open/Save-file portal request and returns the local
  // path once the injected adapter resolves it, or std::nullopt if the
  // request was denied, cancelled, or could not be issued. Both shipped
  // adapters resolve synchronously (a modal dialog or an immediate denial),
  // so this call never returns before the outcome is known.
  [[nodiscard]] std::optional<QString> requestOpenFile();
  [[nodiscard]] std::optional<QString>
  requestSaveFile(const QString &suggestedBaseName);

private:
  QindaQt::AppShell::ApplicationCoordinator m_coordinator;
  std::unique_ptr<FileSelectionAdapter> m_fileAdapter;
};

} // namespace QindaQt::Apps::TextEditor
