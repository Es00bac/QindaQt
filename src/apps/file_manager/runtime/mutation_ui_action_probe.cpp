// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutation_ui_action_probe.h"
#include "selection_ui_probe.h"

#include "model/clipboard_controller.h"
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

#include <optional>

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
  if (index < 0 || !QMetaObject::invokeMethod(list, "selectEntry", Q_ARG(QVariant, QVariant(index)))) {
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

// Clicks a production sort-header button through its QQC2 clicked() signal so
// the probe covers the QML onClicked -> setSortColumn wiring, not just the
// controller. A missing signal fails the probe rather than silently degrading
// to a controller-only sort check.
[[nodiscard]] bool clickObject(QObject *object, const QString &description,
                               QString *error) {
  if (!object ||
      object->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("clicked()")) < 0 ||
      !QMetaObject::invokeMethod(object, "clicked", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not click %1").arg(description));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  return true;
}

[[nodiscard]] std::optional<bool> actionChecked(
    QindaQt::AppShell::ApplicationCoordinator *coordinator, const QString &actionId) {
  const auto actions = coordinator->actionRegistry().actions();
  for (const auto &action : actions) {
    if (action.id == actionId) {
      return action.checked;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<bool> actionEnabled(
    QindaQt::AppShell::ApplicationCoordinator *coordinator, const QString &actionId) {
  const auto actions = coordinator->actionRegistry().actions();
  for (const auto &action : actions) {
    if (action.id == actionId) {
      return action.enabled;
    }
  }
  return std::nullopt;
}

// S3 stage: clipboard copy/paste and cut/paste through the production
// action seam. Fresh seeds keep the stage independent of the S1/S2 fixture
// state. Paste destination follows the focused folder entry, exactly
// as Main.qml routes it.
[[nodiscard]] bool verifyClipboardActionStage(
    QObject *list, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    NavigationController *navigation, MutationController *mutation,
    ClipboardController *clipboard, const QString &fixtureRoot, QString *error) {
  const QString clipFolder = QDir(fixtureRoot).filePath(QStringLiteral("qml-dest"));
  const QString clipSource = QDir(fixtureRoot).filePath(QStringLiteral("qml-clip.txt"));
  const QString clipNested = QDir(clipFolder).filePath(QStringLiteral("qml-clip.txt"));
  if (!QDir().mkpath(clipFolder)) {
    return fail(error, QStringLiteral("could not create the S3 fixture folder"));
  }
  QFile clipSeed(clipSource);
  if (!clipSeed.open(QIODevice::WriteOnly) ||
      clipSeed.write(QByteArray("fixture-clip")) != qint64(12)) {
    return fail(error, QStringLiteral("could not seed the S3 fixture file"));
  }
  clipSeed.close();
  navigation->refresh();
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

  const auto enabledIs = [coordinator](const QString &id, bool expected) {
    return actionEnabled(coordinator, id) == std::optional<bool>(expected);
  };
  if (!enabledIs(QStringLiteral("edit.paste"), false) ||
      !selectEntry(list, navigation, QStringLiteral("qml-clip.txt"), error) ||
      !coordinator->activateAction(QStringLiteral("edit.copy"))) {
    return fail(error, QStringLiteral("could not dispatch the production Copy action"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (!clipboard->canPaste() || clipboard->mode() != QLatin1String("copy") ||
      clipboard->count() != 1 ||
      !enabledIs(QStringLiteral("edit.paste"), true)) {
    return fail(error, QStringLiteral("the Copy action did not arm Paste"));
  }
  if (!selectEntry(list, navigation, QStringLiteral("qml-dest"), error) ||
      !coordinator->activateAction(QStringLiteral("edit.paste")) ||
      !waitForIdle(mutation, error) ||
      readAll(clipNested) != QByteArray("fixture-clip") ||
      readAll(clipSource) != QByteArray("fixture-clip")) {
    return fail(error, QStringLiteral("production QML copy-paste did not commit"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (clipboard->mode() != QLatin1String("copy") ||
      !enabledIs(QStringLiteral("edit.paste"), true)) {
    return fail(error, QStringLiteral("a copy-paste must keep its clipboard snapshot"));
  }

  // Clear the copy-paste payload so the cut stage exercises a clean move.
  if (!QFile::remove(clipNested)) {
    return fail(error, QStringLiteral("could not reset the S3 paste destination"));
  }

  if (!selectEntry(list, navigation, QStringLiteral("qml-clip.txt"), error) ||
      !coordinator->activateAction(QStringLiteral("edit.cut"))) {
    return fail(error, QStringLiteral("could not dispatch the production Cut action"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (clipboard->mode() != QLatin1String("cut")) {
    return fail(error,
                QStringLiteral("the Cut action did not arm a cut paste (mode %1, rejection: %2, selectionCount %3)")
                    .arg(clipboard->mode(), clipboard->lastRejection())
                    .arg(clipboard->selectionCount()));
  }
  if (!selectEntry(list, navigation, QStringLiteral("qml-dest"), error) ||
      !coordinator->activateAction(QStringLiteral("edit.paste")) ||
      !waitForIdle(mutation, error) ||
      QFileInfo::exists(clipSource) ||
      readAll(clipNested) != QByteArray("fixture-clip")) {
    return fail(error, QStringLiteral("production QML cut-paste did not move the file"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (clipboard->canPaste() ||
      !enabledIs(QStringLiteral("edit.paste"), false)) {
    return fail(error, QStringLiteral("a committed cut-paste must clear the clipboard"));
  }
  return true;
}

} // namespace

bool verifyMutationUiActions(
    QObject *root, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    NavigationController *navigation, MutationController *mutation,
    ClipboardController *clipboard, const QString &fixtureRoot, QString *error) {
  // AGENT-NOTE: This is the end-to-end regression for review P1-1. It must
  // retain the production AppShell -> Main.qml -> MutationDialogs.qml ->
  // MutationController path so 64-bit identity marshalling cannot regress
  // behind controller-only tests.
  if (!root || !coordinator || !navigation || !mutation || !clipboard) {
    return fail(error, QStringLiteral("the UI action probe is missing a collaborator"));
  }
  if (!verifySelectionUi(root, navigation, fixtureRoot, error)) return false;
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

  // S2 stage: hidden-toggle round trip, header-driven sort switch, and a
  // multi-item batch trash — all through the same production action seam.
  const QString batch = QDir(fixtureRoot).filePath(QStringLiteral("qml-batch.txt"));
  const QString hidden = QDir(fixtureRoot).filePath(QStringLiteral(".qml-hidden.txt"));
  if (!QFileInfo::exists(batch) || !QFileInfo::exists(hidden)) {
    return fail(error, QStringLiteral("the UI action fixture is missing its S2 seeds"));
  }
  if (navigation->showHidden() || navigation->entryCount() != 3 ||
      navigation->indexOfName(QStringLiteral(".qml-hidden.txt")) >= 0 ||
      navigation->statusMessage() != QStringLiteral("1 hidden")) {
    return fail(error, QStringLiteral("hidden entries were not filtered by default"));
  }
  if (!coordinator->activateAction(QStringLiteral("view.show-hidden"))) {
    return fail(error, QStringLiteral("could not dispatch the Show Hidden action"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (!navigation->showHidden() || navigation->entryCount() != 4 ||
      navigation->indexOfName(QStringLiteral(".qml-hidden.txt")) < 0 ||
      actionChecked(coordinator, QStringLiteral("view.show-hidden")) !=
          std::optional<bool>(true)) {
    return fail(error,
                QStringLiteral("the Show Hidden action did not republish the listing"));
  }
  if (!coordinator->activateAction(QStringLiteral("view.show-hidden"))) {
    return fail(error, QStringLiteral("could not re-dispatch the Show Hidden action"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (navigation->showHidden() || navigation->entryCount() != 3 ||
      actionChecked(coordinator, QStringLiteral("view.show-hidden")) !=
          std::optional<bool>(false)) {
    return fail(error, QStringLiteral("the Show Hidden action did not round-trip"));
  }

  QObject *sortHeaderSize =
      root->findChild<QObject *>(QStringLiteral("sortHeader_size"));
  // Sizes: qml-copy.txt and qml-moved.txt hold 12 bytes, qml-batch.txt 21, so
  // ascending puts the batch file last and descending puts it first; equal
  // sizes keep the ascending name tiebreak.
  if (!clickObject(sortHeaderSize, QStringLiteral("the size sort header"), error) ||
      navigation->sortColumn() != QLatin1String("size") ||
      navigation->sortDirection() != QLatin1String("ascending") ||
      navigation->indexOfName(QStringLiteral("qml-batch.txt")) != 2) {
    return fail(error, QStringLiteral("the size sort header did not reorder ascending"));
  }
  if (!clickObject(sortHeaderSize, QStringLiteral("the size sort header"), error) ||
      navigation->sortDirection() != QLatin1String("descending") ||
      navigation->indexOfName(QStringLiteral("qml-batch.txt")) != 0) {
    return fail(error, QStringLiteral("the size sort header did not toggle descending"));
  }
  navigation->setSortColumn(QStringLiteral("bogus"));
  if (navigation->sortColumn() != QLatin1String("size") ||
      navigation->sortDirection() != QLatin1String("descending")) {
    return fail(error, QStringLiteral("an unknown sort key was not ignored"));
  }

  if (!coordinator->activateAction(QStringLiteral("edit.select-all")) ||
      !coordinator->activateAction(QStringLiteral("file.trash"))) {
    return fail(error, QStringLiteral("could not dispatch the batch Trash actions"));
  }
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  if (!acceptDialog(trashDialog, error) || !waitForIdle(mutation, error) ||
      QFileInfo::exists(copied) || QFileInfo::exists(moved) ||
      QFileInfo::exists(batch) || !QFileInfo::exists(hidden) ||
      mutation->resultText() != QStringLiteral("Finished 3 items") ||
      mutation->canUndo() || mutation->canRestore()) {
    return fail(error, QStringLiteral("production QML batch Trash did not commit"));
  }

  // S3 stage: clipboard copy/paste and cut/paste through the production
  // action seam, verified in its own function for the source-shape budget.
  return verifyClipboardActionStage(list, coordinator, navigation, mutation,
                                    clipboard, fixtureRoot, error);
}

} // namespace QindaQt::Apps::FileManager
