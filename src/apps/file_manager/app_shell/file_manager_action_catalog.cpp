// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_manager_action_catalog.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/application_menu_export.h>
#include <qindaqt/app_shell/menu_export/window_menu_identity.h>

#include <QDBusConnection>
#include <QKeySequence>
#include <QWindow>

#include <cstdio>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] QindaQt::AppShell::ActionSpec action(
    const QString &id, const QString &menuId, const QString &menuLabel,
    const QString &label, const QString &description, const QKeySequence &shortcut,
    int menuOrder, int order, bool destructive = false) {
  return {.id = id,
          .menuId = menuId,
          .menuLabel = menuLabel,
          .label = label,
          .accessibleDescription = description,
          .shortcut = shortcut,
          .menuOrder = menuOrder,
          .order = order,
          .enabled = true,
          .checkable = false,
          .checked = false,
          .destructive = destructive};
}

} // namespace

QList<QindaQt::AppShell::ActionSpec> fileManagerActionCatalog() {
  return {
      action(QStringLiteral("file.new-folder"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("New Folder"),
             QStringLiteral("Create a folder in the current location"),
             QKeySequence(QStringLiteral("Ctrl+Shift+N")), 0, 0),
      action(QStringLiteral("file.rename"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Rename"),
             QStringLiteral("Rename the selected item"),
             QKeySequence(QStringLiteral("F2")), 0, 1),
      action(QStringLiteral("file.copy"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Copy To…"),
             QStringLiteral("Copy the selected item to a local path"),
             QKeySequence(QStringLiteral("Ctrl+Shift+C")), 0, 2),
      action(QStringLiteral("file.move"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Move To…"),
             QStringLiteral("Move the selected item to a local path"),
             QKeySequence(QStringLiteral("Ctrl+Shift+M")), 0, 3),
      action(QStringLiteral("file.trash"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Move to Trash"),
             QStringLiteral("Move the selected item to the recoverable home Trash"),
             QKeySequence(QStringLiteral("Delete")), 0, 4, true),
      action(QStringLiteral("file.restore-last"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Restore Last Trashed Item"),
             QStringLiteral("Restore the last item moved to Trash"),
             QKeySequence(QStringLiteral("Ctrl+Shift+R")), 0, 5),
      action(QStringLiteral("file.empty-trash"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Empty Trash"),
             QStringLiteral("Permanently remove every item from the home Trash"),
             QKeySequence(QStringLiteral("Ctrl+Shift+Delete")), 0, 6, true),
      action(QStringLiteral("edit.undo"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Undo File Operation"),
             QStringLiteral("Undo the last recoverable create, rename, or move"),
             QKeySequence::Undo, 1, 0),
      action(QStringLiteral("operation.cancel"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Cancel Operation"),
             QStringLiteral("Request cancellation of the running file operation"),
             QKeySequence(QStringLiteral("Ctrl+Escape")), 1, 1),
  };
}

std::unique_ptr<QObject> composeFileManagerMenuExport(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, QObject *qmlRoot) {
  auto *window = qobject_cast<QWindow *>(qmlRoot);
  if (window == nullptr) {
    return {};
  }
#if defined(QINDAQT_ENABLE_FILE_MANAGER_MENU_EXPORT_TEST_SEAM)
  class TestIdentityPublisher final
      : public QindaQt::AppShell::MenuExport::WindowMenuIdentityPublisher {
  public:
    explicit TestIdentityPublisher(quint32 id) : m_id(id) {}
    std::optional<QindaQt::AppShell::MenuExport::WindowMenuIdentity>
    publish(QWindow &, const QString &, const QString &) override {
      return QindaQt::AppShell::MenuExport::WindowMenuIdentity{
          .kind =
              QindaQt::AppShell::MenuExport::WindowMenuIdentityKind::XWindow,
          .registrarWindowId = m_id};
    }
    void withdraw(QWindow &) override {}

  private:
    quint32 m_id;
  };
  bool idOk = false;
  const int testWindowIdValue =
      qEnvironmentVariableIntValue("QINDAQT_TEST_APPMENU_WINDOW_ID", &idOk);
  if (idOk && testWindowIdValue > 0) {
    const auto testWindowId = static_cast<quint32>(testWindowIdValue);
    if (qEnvironmentVariableIsSet("QINDAQT_TEST_APPMENU_TRACE_ACTIVATION")) {
      QObject::connect(
          &coordinator,
          &QindaQt::AppShell::ApplicationCoordinator::actionRequested,
          &coordinator, [](const QString &actionId) {
            std::fprintf(stdout, "ACTIVATED %s\n", qPrintable(actionId));
            std::fflush(stdout);
          });
    }
    auto composition =
        std::make_unique<QindaQt::AppShell::MenuExport::ApplicationMenuExport>(
            coordinator, *window, QDBusConnection::sessionBus(),
            std::make_unique<TestIdentityPublisher>(testWindowId));
    (void)composition->start();
    return std::unique_ptr<QObject>(std::move(composition));
  }
#endif
  const QDBusConnection sessionBus = QDBusConnection::sessionBus();
  if (!sessionBus.isConnected()) {
    return {};
  }
  return std::unique_ptr<QObject>(
      QindaQt::AppShell::MenuExport::ApplicationMenuExport::compose(
          coordinator, *window, sessionBus));
}

} // namespace QindaQt::Apps::FileManager
