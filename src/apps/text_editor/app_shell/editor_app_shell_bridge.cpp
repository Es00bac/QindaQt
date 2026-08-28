// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_app_shell_bridge.h"

#include "editor_action_catalog.h"
#include "fail_closed_file_selection_adapter.h"

#include <QAction>
#include <QUrl>

namespace QindaQt::Apps::TextEditor {

using QindaQt::AppShell::ApplicationCoordinator;
using QindaQt::AppShell::Error;
using QindaQt::AppShell::ErrorCode;
using QindaQt::AppShell::makeError;
using QindaQt::AppShell::PortalKind;
using QindaQt::AppShell::PortalRequest;
using QindaQt::AppShell::PortalResult;

EditorAppShellBridge::EditorAppShellBridge(
    std::unique_ptr<FileSelectionAdapter> fileAdapter, QObject *parent)
    : QObject(parent),
      m_fileAdapter(fileAdapter ? std::move(fileAdapter)
                                : std::make_unique<
                                      FailClosedFileSelectionAdapter>()) {
  connect(&m_coordinator, &ApplicationCoordinator::portalRequestIssued, this,
          [this](const PortalRequest &request) {
            switch (request.kind) {
            case PortalKind::OpenFile:
              m_fileAdapter->presentOpenFile(m_coordinator, request);
              return;
            case PortalKind::SaveFile:
              m_fileAdapter->presentSaveFile(m_coordinator, request);
              return;
            case PortalKind::SelectFolder:
              // The editor never requests a folder; fail closed rather than
              // leave a request pending if one is ever issued by mistake.
              (void)m_coordinator.resolvePortal(
                  request.id, false, {},
                  makeError(ErrorCode::Denied,
                           QStringLiteral(
                               "Folder selection is not supported"),
                           false));
              return;
            }
          });
}

ApplicationCoordinator &EditorAppShellBridge::coordinator() {
  return m_coordinator;
}

Error EditorAppShellBridge::publishActionCatalog() {
  return m_coordinator.replaceActions(editorActionCatalog());
}

Error EditorAppShellBridge::setActionEnabled(const QString &actionId,
                                             bool enabled) {
  return m_coordinator.setActionEnabled(actionId, enabled);
}

void EditorAppShellBridge::bindActivationTargets(
    const QHash<QString, QAction *> &targets) {
  connect(&m_coordinator, &ApplicationCoordinator::actionRequested, this,
          [targets](const QString &actionId) {
            if (QAction *action = targets.value(actionId)) {
              action->trigger();
            }
          });
}

std::optional<QString> EditorAppShellBridge::requestOpenFile() {
  std::optional<QString> resolved;
  const QMetaObject::Connection connection = connect(
      &m_coordinator, &ApplicationCoordinator::portalFinished, this,
      [&resolved](const PortalResult &result) {
        if (result.accepted && !result.urls.isEmpty()) {
          resolved = result.urls.first().toLocalFile();
        }
      });
  const quint64 requestId = m_coordinator.requestOpenFile(
      QStringLiteral("Open text document"));
  disconnect(connection);
  if (requestId == 0) {
    return std::nullopt;
  }
  return resolved;
}

std::optional<QString>
EditorAppShellBridge::requestSaveFile(const QString &suggestedBaseName) {
  std::optional<QString> resolved;
  const QMetaObject::Connection connection = connect(
      &m_coordinator, &ApplicationCoordinator::portalFinished, this,
      [&resolved](const PortalResult &result) {
        if (result.accepted && !result.urls.isEmpty()) {
          resolved = result.urls.first().toLocalFile();
        }
      });
  const quint64 requestId = m_coordinator.requestSaveFile(
      QStringLiteral("Save text document"), suggestedBaseName);
  disconnect(connection);
  if (requestId == 0) {
    return std::nullopt;
  }
  return resolved;
}

} // namespace QindaQt::Apps::TextEditor
