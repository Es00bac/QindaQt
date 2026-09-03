// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_ui_action_probe.h"

#include "model/navigation_controller.h"
#include "mutation/mutation_controller.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QThread>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] bool fail(QString *error, const QString &message) {
  if (error) {
    *error = message;
  }
  return false;
}

[[nodiscard]] bool waitForIdle(MutationController *mutation, QString *error) {
  QElapsedTimer timer;
  timer.start();
  while (mutation->busy() && timer.elapsed() < 5000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(1);
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (mutation->busy()) {
    return fail(error, QStringLiteral("the mutation action timed out"));
  }
  if (mutation->failureCode() != QLatin1String("none")) {
    return fail(error,
                QStringLiteral("mutation failed with %1: %2")
                    .arg(mutation->failureCode(), mutation->failureMessage()));
  }
  return true;
}

[[nodiscard]] bool selectEntry(QObject *list,
                               NavigationController *navigation,
                               const QString &name, QString *error) {
  const int index = navigation->indexOfName(name);
  if (index < 0 || !list->setProperty("currentIndex", index)) {
    return fail(error, QStringLiteral("could not select fixture entry %1").arg(name));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  return true;
}

[[nodiscard]] bool acceptDialog(QObject *dialog, QString *error) {
  if (!QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not accept production dialog %1")
                           .arg(dialog->objectName()));
  }
  return true;
}

[[nodiscard]] bool dispatchPathAction(
    QObject *list, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    NavigationController *navigation, MutationController *mutation,
    const QString &entryName, const QString &actionId, QObject *dialog,
    QObject *field, const QString &value, QString *error) {
  if (!selectEntry(list, navigation, entryName, error) ||
      !coordinator->activateAction(actionId)) {
    return fail(error, QStringLiteral("could not dispatch production action %1")
                           .arg(actionId));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (field && !field->setProperty("text", value)) {
    return fail(error, QStringLiteral("could not set production dialog field"));
  }
  return acceptDialog(dialog, error) && waitForIdle(mutation, error);
}

[[nodiscard]] QByteArray readAll(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

} // namespace

bool verifyMutationUiActions(
    QObject *root, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    NavigationController *navigation, MutationController *mutation,
    const QString &fixtureRoot, QString *error) {
  // AGENT-NOTE: This is the end-to-end regression for review P1-1. It must
  // retain the production AppShell -> Main.qml -> MutationDialogs.qml ->
  // MutationController path so 64-bit identity marshalling cannot regress
  // behind controller-only tests.
  if (!root || !coordinator || !navigation || !mutation) {
    return fail(error, QStringLiteral("the UI action probe is missing a collaborator"));
  }
  QObject *list = root->findChild<QObject *>(QStringLiteral("entryListView"));
  QObject *renameDialog =
      root->findChild<QObject *>(QStringLiteral("renameDialog"));
  QObject *renameField =
      root->findChild<QObject *>(QStringLiteral("renameNameField"));
  QObject *destinationDialog =
      root->findChild<QObject *>(QStringLiteral("destinationDialog"));
  QObject *destinationField =
      root->findChild<QObject *>(QStringLiteral("destinationPathField"));
  QObject *trashDialog =
      root->findChild<QObject *>(QStringLiteral("trashConfirmationDialog"));
  if (!list || !renameDialog || !renameField || !destinationDialog ||
      !destinationField || !trashDialog) {
    return fail(error, QStringLiteral("the production mutation UI is incomplete"));
  }

  const QString source = QDir(fixtureRoot).filePath(QStringLiteral("qml-source.txt"));
  const QString renamed = QDir(fixtureRoot).filePath(QStringLiteral("qml-renamed.txt"));
  const QString copied = QDir(fixtureRoot).filePath(QStringLiteral("qml-copy.txt"));
  const QString moved = QDir(fixtureRoot).filePath(QStringLiteral("qml-moved.txt"));
  if (!dispatchPathAction(list, coordinator, navigation, mutation,
                          QStringLiteral("qml-source.txt"),
                          QStringLiteral("file.rename"), renameDialog,
                          renameField, QStringLiteral("qml-renamed.txt"), error) ||
      QFileInfo::exists(source) || readAll(renamed) != QByteArray("fixture-data")) {
    return fail(error, QStringLiteral("production QML rename did not commit"));
  }
  if (!dispatchPathAction(list, coordinator, navigation, mutation,
                          QStringLiteral("qml-renamed.txt"),
                          QStringLiteral("file.copy"), destinationDialog,
                          destinationField, copied, error) ||
      readAll(copied) != QByteArray("fixture-data")) {
    return fail(error, QStringLiteral("production QML copy did not commit"));
  }
  if (!dispatchPathAction(list, coordinator, navigation, mutation,
                          QStringLiteral("qml-renamed.txt"),
                          QStringLiteral("file.move"), destinationDialog,
                          destinationField, moved, error) ||
      QFileInfo::exists(renamed) || readAll(moved) != QByteArray("fixture-data")) {
    return fail(error, QStringLiteral("production QML move did not commit"));
  }
  if (!selectEntry(list, navigation, QStringLiteral("qml-moved.txt"), error) ||
      !coordinator->activateAction(QStringLiteral("file.trash"))) {
    return fail(error, QStringLiteral("could not dispatch production Trash action"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (!acceptDialog(trashDialog, error) || !waitForIdle(mutation, error) ||
      QFileInfo::exists(moved) || !mutation->canRestore()) {
    return fail(error, QStringLiteral("production QML Trash did not commit"));
  }
  if (!coordinator->activateAction(QStringLiteral("file.restore-last")) ||
      !waitForIdle(mutation, error) || readAll(moved) != QByteArray("fixture-data")) {
    return fail(error, QStringLiteral("production QML restore did not commit"));
  }
  return true;
}

} // namespace QindaQt::Apps::FileManager
