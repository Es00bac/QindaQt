// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/file_manager_application.h"

#include <QApplication>

std::unique_ptr<QGuiApplication>
QindaQt::Apps::FileManager::createApplication(int &argc, char **argv) {
  // AGENT-GUARD: the application class here must stay QWidget-capable. KIO's
  // registered default job UI delegate (KIOWidgets) installs widget handlers
  // (Open With, untrusted program, ask-user); the Open With handler
  // constructs KOpenWithDialog, which aborts the whole process under a bare
  // QGuiApplication (review P1 on 2b37f9c1). QApplication is used although
  // the UI is Qt Quick (ADR-0116): it is the QGuiApplication subclass that
  // supports QWidget prompts. Returning the result as QGuiApplication is
  // safe: the QObject destructor chain is virtual.
  return std::make_unique<QApplication>(argc, argv);
}
