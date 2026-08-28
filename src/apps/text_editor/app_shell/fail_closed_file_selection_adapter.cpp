// SPDX-License-Identifier: GPL-3.0-or-later
#include "fail_closed_file_selection_adapter.h"

#include "qindaqt/app_shell/application_coordinator.h"

namespace QindaQt::Apps::TextEditor {
namespace {

void denyRequest(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                 const QindaQt::AppShell::PortalRequest &request) {
  using QindaQt::AppShell::ErrorCode;
  using QindaQt::AppShell::makeError;
  (void)coordinator.resolvePortal(
      request.id, false, {},
      makeError(ErrorCode::Denied,
               QStringLiteral("No file-selection adapter is available"),
               false));
}

} // namespace

void FailClosedFileSelectionAdapter::presentOpenFile(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    const QindaQt::AppShell::PortalRequest &request) {
  denyRequest(coordinator, request);
}

void FailClosedFileSelectionAdapter::presentSaveFile(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    const QindaQt::AppShell::PortalRequest &request) {
  denyRequest(coordinator, request);
}

} // namespace QindaQt::Apps::TextEditor
