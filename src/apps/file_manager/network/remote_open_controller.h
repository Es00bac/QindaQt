// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace QindaQt::Apps::FileManager {

class RemoteFileOpener;

// Owns the remote-open result lifecycle so NavigationController does not
// accumulate it (ADR-0152): forwards one open() to the injected opener and
// maps its single openFinished to either cleared() (a success retired the
// previous error) or failure() (bounded diagnostic). The opener -- and any
// KIO job or prompt it owns -- outlives navigation because the controller
// owns both objects; this collaborator adds no policy of its own.
class RemoteOpenController : public QObject {
  Q_OBJECT

public:
  explicit RemoteOpenController(RemoteFileOpener &opener, QObject *parent = nullptr);

  void open(const QUrl &url);

signals:
  void cleared();
  void failure(const QString &message);

private:
  void onOpenFinished(const QString &diagnostic);

  RemoteFileOpener *m_opener;
};

} // namespace QindaQt::Apps::FileManager
