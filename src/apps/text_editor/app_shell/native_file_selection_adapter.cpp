// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_file_selection_adapter.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QFileDialog>
#include <QObject>
#include <QUrl>

namespace QindaQt::Apps::TextEditor {

NativeFileSelectionAdapter::NativeFileSelectionAdapter(QWidget *parent)
    : m_parent(parent) {}

void NativeFileSelectionAdapter::presentOpenFile(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    const QindaQt::AppShell::PortalRequest &request) {
  const QString path = QFileDialog::getOpenFileName(
      m_parent, request.title, {}, QObject::tr("All files (*)"));
  if (path.isEmpty()) {
    (void)coordinator.resolvePortal(request.id, false);
    return;
  }
  (void)coordinator.resolvePortal(request.id, true,
                                  {QUrl::fromLocalFile(path)});
}

void NativeFileSelectionAdapter::presentSaveFile(
    QindaQt::AppShell::ApplicationCoordinator &coordinator,
    const QindaQt::AppShell::PortalRequest &request) {
  const QString path =
      QFileDialog::getSaveFileName(m_parent, request.title,
                                   request.suggestedName,
                                   QObject::tr("All files (*)"));
  if (path.isEmpty()) {
    (void)coordinator.resolvePortal(request.id, false);
    return;
  }
  (void)coordinator.resolvePortal(request.id, true,
                                  {QUrl::fromLocalFile(path)});
}

} // namespace QindaQt::Apps::TextEditor
