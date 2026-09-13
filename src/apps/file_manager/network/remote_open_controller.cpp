// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_open_controller.h"
#include "remote_file_opener.h"

namespace QindaQt::Apps::FileManager {

RemoteOpenController::RemoteOpenController(RemoteFileOpener &opener, QObject *parent)
    : QObject(parent), m_opener(&opener) {
  connect(&opener, &RemoteFileOpener::openFinished, this,
          [this](const QString &diagnostic) { onOpenFinished(diagnostic); });
}

void RemoteOpenController::open(const QUrl &url) { m_opener->open(url); }

void RemoteOpenController::onOpenFinished(const QString &diagnostic) {
  if (diagnostic.isEmpty()) {
    Q_EMIT cleared();
    return;
  }
  Q_EMIT failure(diagnostic);
}

} // namespace QindaQt::Apps::FileManager
