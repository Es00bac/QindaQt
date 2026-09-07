// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <optional>
namespace QindaQt::WorkspacesApps {
struct DesktopApplication {
  QString id;
  QString name;
  QString iconName;
};
// GUI-thread adapter owned by its caller. Uses the XDG application catalog;
// never interprets saved application intent as a shell command. Launch is
// asynchronous: true means dispatched, launchFinished reports process startup,
// and neither means the compositor has received a usable window yet.
class DesktopApplications final : public QObject {
  Q_OBJECT
public:
  explicit DesktopApplications(QObject *parent = nullptr);
  [[nodiscard]] std::optional<DesktopApplication>
  find(const QString &desktopEntryId) const;
  [[nodiscard]] bool launch(const QString &desktopEntryId,
                            const QStringList &urls,
                            const QByteArray &activationToken = {},
                            QString *error = nullptr);
Q_SIGNALS:
  void launchFinished(const QString &desktopEntryId, bool started,
                      const QString &error);
};
} // namespace QindaQt::WorkspacesApps
